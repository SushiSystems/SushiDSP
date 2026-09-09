/**************************************************************************/
/* BandResponseFit.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file BandResponseFit.cpp
 * @brief A two-shelf closed-form guess refined by damped Gauss-Newton steps toward a prior.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/BandResponseFit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Counts the free parameters: a flat gain, then gain, corner and Q of each shelf.
        constexpr std::size_t kParameterCount = 7;

        /// @brief Bounds the refinement at eight accepted steps.
        constexpr int kMaxSteps = 8;

        /// @brief Stops the refinement once an accepted step moves the parameter vector by less
        /// than this, in its own units of decibels and natural logarithms.
        constexpr double kStepTolerance = 1e-3;

        /// @brief Weights the pull of every parameter toward the closed-form guess, in squared
        /// decibels of cost per squared unit of the parameter.
        constexpr double kPriorWeight = 0.1;

        /// @brief Holds the parameter vector the refinement moves.
        using Parameters = std::array<double, kParameterCount>;

        /// @brief Holds one section's five normalised coefficients.
        using Section = std::array<double, 5>;

        /// @brief Holds one value per band centre.
        using Bands = std::array<double, kBandCount>;

        /// @brief Names each slot of the parameter vector.
        enum Slot : std::size_t
        {
            kFlatDb = 0,
            kLowDb,
            kLowLogCorner,
            kLowLogQ,
            kHighDb,
            kHighLogCorner,
            kHighLogQ
        };

        /// @brief Returns the RBJ shelf coefficients, a low shelf when low is true.
        Section shelf(double sample_rate, double corner_hz, double gain_db, double q, bool low)
        {
            const double a = std::pow(10.0, gain_db / 40.0);
            const double omega = 2.0 * kPi * corner_hz / sample_rate;
            const double sin_omega = std::sin(omega);
            const double cos_omega = low ? std::cos(omega) : -std::cos(omega);
            const double alpha = sin_omega / (2.0 * q);
            const double two_sqrt_a_alpha = 2.0 * std::sqrt(a) * alpha;
            const double sign = low ? 1.0 : -1.0;
            const double b0 = a * ((a + 1.0) - (a - 1.0) * cos_omega + two_sqrt_a_alpha);
            const double b1 = sign * 2.0 * a * ((a - 1.0) - (a + 1.0) * cos_omega);
            const double b2 = a * ((a + 1.0) - (a - 1.0) * cos_omega - two_sqrt_a_alpha);
            const double a0 = (a + 1.0) + (a - 1.0) * cos_omega + two_sqrt_a_alpha;
            const double a1 = sign * -2.0 * ((a - 1.0) + (a + 1.0) * cos_omega);
            const double a2 = (a + 1.0) + (a - 1.0) * cos_omega - two_sqrt_a_alpha;
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }

        /// @brief Returns the low shelf the parameter vector describes.
        Section low_shelf(const Parameters& parameters, double sample_rate)
        {
            return shelf(sample_rate, std::exp(parameters[kLowLogCorner]), parameters[kLowDb],
                         std::exp(parameters[kLowLogQ]), true);
        }

        /// @brief Returns the high shelf the parameter vector describes.
        Section high_shelf(const Parameters& parameters, double sample_rate)
        {
            return shelf(sample_rate, std::exp(parameters[kHighLogCorner]), parameters[kHighDb],
                         std::exp(parameters[kHighLogQ]), false);
        }

        /// @brief Returns true when a section's poles lie inside the unit circle.
        bool is_stable(const Section& section) noexcept
        {
            const double a1 = section[3];
            const double a2 = section[4];
            return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
        }

        /// @brief Returns a section's gain at zero frequency, (b0 + b1 + b2) / (1 + a1 + a2).
        double dc_gain(const Section& section) noexcept
        {
            return (section[0] + section[1] + section[2]) / (1.0 + section[3] + section[4]);
        }

        /// @brief Builds the canonical cascade: the low shelf first, the high shelf second with
        /// unity gain at zero frequency, the whole gain in the first section's numerator.
        BandResponseFit::Coefficients realise(const Section& low, const Section& high,
                                              double flat_db) noexcept
        {
            BandResponseFit::Coefficients coefficients{ low, high };
            const double high_dc = dc_gain(high);
            const double gain = std::pow(10.0, flat_db / 20.0) * high_dc;
            for (std::size_t index = 0; index < 3; ++index)
            {
                coefficients.section_a[index] *= gain;
                coefficients.section_b[index] /= high_dc;
            }
            return coefficients;
        }

        /// @brief Returns the target's six log-magnitudes in decibels, floored at kDecibelFloor.
        Bands target_db(const BandResponse& target) noexcept
        {
            Bands result{};
            const std::array<float, kBandCount> decibels = target.decibels();
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                result[band] = static_cast<double>(decibels[band]);
            }
            return result;
        }

        /// @brief Holds cos(w) and cos(2w) at each band centre for one sample rate.
        struct BandGrid
        {
            /// @brief Stores cos(w) per band.
            Bands cosine;

            /// @brief Stores cos(2w) per band.
            Bands double_cosine;
        };

        /// @brief Evaluates the band grid of one sample rate.
        BandGrid band_grid(double sample_rate) noexcept
        {
            BandGrid grid{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double omega = 2.0 * kPi * kBandCentresHz[band] / sample_rate;
                grid.cosine[band] = std::cos(omega);
                grid.double_cosine[band] = std::cos(2.0 * omega);
            }
            return grid;
        }

        /// @brief Returns |c0 + c1 z^-1 + c2 z^-2|^2 on the unit circle from cos(w) and cos(2w).
        double squared_magnitude(double c0, double c1, double c2, double cosine,
                                 double double_cosine) noexcept
        {
            return c0 * c0 + c1 * c1 + c2 * c2 + 2.0 * c1 * (c0 + c2) * cosine
                + 2.0 * c0 * c2 * double_cosine;
        }

        /// @brief Returns one section's squared magnitude at one grid point.
        double power_gain(const Section& section, double cosine, double double_cosine) noexcept
        {
            const double numerator =
                squared_magnitude(section[0], section[1], section[2], cosine, double_cosine);
            const double denominator =
                squared_magnitude(1.0, section[3], section[4], cosine, double_cosine);
            return numerator / denominator;
        }

        /// @brief Returns one section's magnitude in decibels at every band centre.
        Bands section_db(const Section& section, const BandGrid& grid) noexcept
        {
            Bands result{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double power =
                    power_gain(section, grid.cosine[band], grid.double_cosine[band]);
                result[band] = 10.0 * std::log10(std::max(power, 1e-20));
            }
            return result;
        }

        /// @brief Returns the largest absolute value of six band values.
        double worst(const Bands& values) noexcept
        {
            double largest = 0.0;
            for (const double value : values)
            {
                largest = std::max(largest, std::fabs(value));
            }
            return largest;
        }

        /// @brief Returns the Euclidean distance between two parameter vectors.
        double distance(const Parameters& a, const Parameters& b) noexcept
        {
            double sum = 0.0;
            for (std::size_t slot = 0; slot < kParameterCount; ++slot)
            {
                const double offset = a[slot] - b[slot];
                sum += offset * offset;
            }
            return std::sqrt(sum);
        }

        /// @brief Holds everything one parameter vector evaluates to.
        struct Evaluation
        {
            /// @brief Stores the low shelf's coefficients.
            Section low;

            /// @brief Stores the high shelf's coefficients.
            Section high;

            /// @brief Stores the low shelf's magnitude per band, in decibels.
            Bands low_db;

            /// @brief Stores the high shelf's magnitude per band, in decibels.
            Bands high_db;

            /// @brief Stores the cascade's magnitude minus the target per band, in decibels.
            Bands residual;

            /// @brief Stores the squared residuals plus the weighted squared distance from the
            /// prior.
            double cost;

            /// @brief Stores whether both shelves have their poles inside the unit circle.
            bool stable;
        };

        /// @brief Evaluates a parameter vector against the target and the prior.
        Evaluation evaluate(const Parameters& parameters, const Parameters& prior,
                            const Bands& wanted, const BandGrid& grid, double sample_rate)
        {
            Evaluation result{};
            result.low = low_shelf(parameters, sample_rate);
            result.high = high_shelf(parameters, sample_rate);
            result.low_db = section_db(result.low, grid);
            result.high_db = section_db(result.high, grid);
            result.cost = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                result.residual[band] = parameters[kFlatDb] + result.low_db[band]
                    + result.high_db[band] - wanted[band];
                result.cost += result.residual[band] * result.residual[band];
            }
            for (std::size_t slot = 0; slot < kParameterCount; ++slot)
            {
                const double offset = parameters[slot] - prior[slot];
                result.cost += kPriorWeight * offset * offset;
            }
            result.stable = is_stable(result.low) && is_stable(result.high);
            return result;
        }

        /// @brief Holds the residuals' derivative with respect to each parameter, one row per
        /// slot.
        using Jacobian = std::array<Bands, kParameterCount>;

        /// @brief Returns the Jacobian at an evaluation: the flat gain's row is one, each shelf's
        /// three rows are forward differences of that shelf alone.
        Jacobian jacobian(const Parameters& parameters, const Evaluation& at,
                          const BandGrid& grid, double sample_rate)
        {
            constexpr double delta = 1e-4;
            Jacobian result{};
            result[kFlatDb].fill(1.0);
            for (std::size_t slot = kLowDb; slot < kParameterCount; ++slot)
            {
                const bool low = slot <= kLowLogQ;
                Parameters nudged = parameters;
                nudged[slot] += delta;
                const Bands shifted = section_db(
                    low ? low_shelf(nudged, sample_rate) : high_shelf(nudged, sample_rate), grid);
                const Bands& base = low ? at.low_db : at.high_db;
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    result[slot][band] = (shifted[band] - base[band]) / delta;
                }
            }
            return result;
        }

        /// @brief Solves the symmetric positive-definite system normal * step = right_hand_side by
        /// Gaussian elimination with partial pivoting; returns false when it is singular.
        bool solve(std::array<Parameters, kParameterCount> normal, Parameters right_hand_side,
                   Parameters& step) noexcept
        {
            for (std::size_t column = 0; column < kParameterCount; ++column)
            {
                std::size_t pivot = column;
                for (std::size_t row = column + 1; row < kParameterCount; ++row)
                {
                    if (std::fabs(normal[row][column]) > std::fabs(normal[pivot][column]))
                    {
                        pivot = row;
                    }
                }
                if (std::fabs(normal[pivot][column]) < 1e-14)
                {
                    return false;
                }
                std::swap(normal[pivot], normal[column]);
                std::swap(right_hand_side[pivot], right_hand_side[column]);
                for (std::size_t row = column + 1; row < kParameterCount; ++row)
                {
                    const double factor = normal[row][column] / normal[column][column];
                    for (std::size_t inner = column; inner < kParameterCount; ++inner)
                    {
                        normal[row][inner] -= factor * normal[column][inner];
                    }
                    right_hand_side[row] -= factor * right_hand_side[column];
                }
            }
            for (std::size_t back = kParameterCount; back-- > 0;)
            {
                double sum = right_hand_side[back];
                for (std::size_t inner = back + 1; inner < kParameterCount; ++inner)
                {
                    sum -= normal[back][inner] * step[inner];
                }
                step[back] = sum / normal[back][back];
            }
            return true;
        }

        /// @brief Solves one damped Gauss-Newton step of the prior-weighted problem; returns
        /// false when the damped normal matrix is singular.
        bool damped_step(const Jacobian& slope, const Evaluation& at, const Parameters& parameters,
                         const Parameters& prior, double damping, Parameters& step) noexcept
        {
            std::array<Parameters, kParameterCount> normal{};
            Parameters gradient{};
            for (std::size_t row = 0; row < kParameterCount; ++row)
            {
                for (std::size_t column = 0; column < kParameterCount; ++column)
                {
                    double sum = 0.0;
                    for (std::size_t band = 0; band < kBandCount; ++band)
                    {
                        sum += slope[row][band] * slope[column][band];
                    }
                    normal[row][column] = sum;
                }
                normal[row][row] += kPriorWeight;
                normal[row][row] *= 1.0 + damping;
                double sum = -kPriorWeight * (parameters[row] - prior[row]);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    sum -= slope[row][band] * at.residual[band];
                }
                gradient[row] = sum;
            }
            return solve(normal, gradient, step);
        }

        /// @brief Keeps every corner inside (20 Hz, 0.45 * sample_rate) and every Q inside
        /// (0.25, 4).
        void clamp(Parameters& parameters, double sample_rate) noexcept
        {
            const double lowest = std::log(20.0);
            const double highest = std::log(0.45 * sample_rate);
            parameters[kLowLogCorner] = std::clamp(parameters[kLowLogCorner], lowest, highest);
            parameters[kHighLogCorner] = std::clamp(parameters[kHighLogCorner], lowest, highest);
            parameters[kLowLogQ] = std::clamp(parameters[kLowLogQ], std::log(0.25), std::log(4.0));
            parameters[kHighLogQ] = std::clamp(parameters[kHighLogQ], std::log(0.25), std::log(4.0));
        }

        /// @brief Returns the closed-form two-shelf guess: corners at 250 Hz and 2 kHz, Q 0.7071,
        /// the flat gain at the mean of the two middle bands.
        Parameters initial_guess(const Bands& wanted) noexcept
        {
            Parameters parameters{};
            parameters[kFlatDb] = 0.5 * (wanted[2] + wanted[3]);
            parameters[kLowDb] = wanted[0] - parameters[kFlatDb];
            parameters[kLowLogCorner] = std::log(250.0);
            parameters[kLowLogQ] = std::log(0.7071);
            parameters[kHighDb] = wanted[5] - parameters[kFlatDb];
            parameters[kHighLogCorner] = std::log(2000.0);
            parameters[kHighLogQ] = std::log(0.7071);
            return parameters;
        }
    }

    double BandResponseFit::magnitude_db(const Coefficients& coefficients, double frequency_hz,
                                         double sample_rate)
    {
        const double omega = 2.0 * kPi * frequency_hz / sample_rate;
        const double cosine = std::cos(omega);
        const double double_cosine = std::cos(2.0 * omega);
        const double power = power_gain(coefficients.section_a, cosine, double_cosine)
            * power_gain(coefficients.section_b, cosine, double_cosine);
        return 10.0 * std::log10(std::max(power, 1e-20));
    }

    BandResponseFit::Result BandResponseFit::fit(const BandResponse& target, double sample_rate)
    {
        const Bands wanted = target_db(target);
        Parameters prior = initial_guess(wanted);
        clamp(prior, sample_rate);
        const BandGrid grid = band_grid(sample_rate);
        Parameters parameters = prior;
        Evaluation best = evaluate(parameters, prior, wanted, grid, sample_rate);
        double damping = 1e-3;
        for (int step_index = 0; step_index < kMaxSteps; ++step_index)
        {
            const Jacobian slope = jacobian(parameters, best, grid, sample_rate);
            bool accepted = false;
            double step_length = 0.0;
            for (int attempt = 0; attempt < 6 && !accepted; ++attempt)
            {
                Parameters step{};
                if (!damped_step(slope, best, parameters, prior, damping, step))
                {
                    damping *= 10.0;
                    continue;
                }
                Parameters candidate = parameters;
                for (std::size_t slot = 0; slot < kParameterCount; ++slot)
                {
                    candidate[slot] += step[slot];
                }
                clamp(candidate, sample_rate);
                const Evaluation trial = evaluate(candidate, prior, wanted, grid, sample_rate);
                if (trial.stable && trial.cost < best.cost)
                {
                    step_length = distance(candidate, parameters);
                    parameters = candidate;
                    best = trial;
                    damping = std::max(damping * 0.1, 1e-6);
                    accepted = true;
                }
                else
                {
                    damping *= 10.0;
                }
            }
            if (!accepted || step_length < kStepTolerance)
            {
                break;
            }
        }
        return Result{ realise(best.low, best.high, parameters[kFlatDb]), worst(best.residual) };
    }
}
