/**************************************************************************/
/* BandResponseFit.cpp                                                    */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file BandResponseFit.cpp
 * @brief Two sections that are each a shelf, a peak or both, refined by damped Gauss-Newton
 * steps from a two-shelf closed-form guess toward the same guess as a prior.
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

        /// @brief Counts the free parameters: a flat gain, then the log corner, step, peak and
        /// log Q of each section.
        constexpr std::size_t kParameterCount = 9;

        /// @brief Counts the parameters of one section.
        constexpr std::size_t kSectionSlots = 4;

        /// @brief Bounds the refinement at eight accepted steps.
        constexpr int kMaxSteps = 8;

        /// @brief Stops the refinement once an accepted step moves the parameter vector by less
        /// than this, in its own units of decibels and natural logarithms.
        constexpr double kStepTolerance = 1e-3;

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
            kFirstLogCorner,
            kFirstStepDb,
            kFirstPeakDb,
            kFirstLogQ,
            kSecondLogCorner,
            kSecondStepDb,
            kSecondPeakDb,
            kSecondLogQ
        };

        /// @brief Weights the pull of each parameter toward the closed-form guess, in squared
        /// decibels of cost per squared unit of the parameter: 0.1 for the gains, corners and
        /// Qs, 0.01 for the two peaks. Spec §4.2.
        constexpr Parameters kPriorWeights = { 0.1, 0.1, 0.1, 0.01, 0.1, 0.1, 0.1, 0.01, 0.1 };

        /// @brief Returns the three coefficients of a root pair at the given frequency and Q,
        /// bilinear-transformed and scaled to a value of one at zero frequency.
        std::array<double, 3> root_pair(double sample_rate, double frequency_hz, double q) noexcept
        {
            const double k = std::tan(kPi * frequency_hz / sample_rate);
            const double k_squared = k * k;
            const double scale = 1.0 / (4.0 * k_squared);
            return { scale * (1.0 + k / q + k_squared), scale * 2.0 * (k_squared - 1.0),
                     scale * (1.0 - k / q + k_squared) };
        }

        /// @brief Returns the section held in the four slots from the given one, unity gain at
        /// zero frequency and a0 equal to one: its zero pair sits below the corner and its pole
        /// pair above it by the step, and its zero Q sits below the Q and its pole Q above it by
        /// the peak, so that the section rises by the step from zero frequency to half the
        /// sample rate and by the peak at the corner.
        Section section_at(const Parameters& parameters, std::size_t first, double sample_rate)
        {
            const double corner_hz = std::exp(parameters[first]);
            const double step_ratio = std::pow(10.0, parameters[first + 1] / 80.0);
            const double peak_ratio = std::pow(10.0, parameters[first + 2] / 40.0);
            const double q = std::exp(parameters[first + 3]);
            const std::array<double, 3> zeros =
                root_pair(sample_rate, corner_hz / step_ratio, q / peak_ratio);
            const std::array<double, 3> poles =
                root_pair(sample_rate, corner_hz * step_ratio, q * peak_ratio);
            return { zeros[0] / poles[0], zeros[1] / poles[0], zeros[2] / poles[0],
                     poles[1] / poles[0], poles[2] / poles[0] };
        }

        /// @brief Returns true when a section's poles lie inside the unit circle.
        bool is_stable(const Section& section) noexcept
        {
            const double a1 = section[3];
            const double a2 = section[4];
            return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
        }

        /// @brief Builds the canonical cascade: the second section at unity gain at zero
        /// frequency as parameterised, the whole gain in the first section's numerator.
        BandResponseFit::Coefficients realise(const Section& first, const Section& second,
                                              double flat_db) noexcept
        {
            BandResponseFit::Coefficients coefficients{ first, second };
            const double gain = std::pow(10.0, flat_db / 20.0);
            for (std::size_t index = 0; index < 3; ++index)
            {
                coefficients.section_a[index] *= gain;
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
            /// @brief Stores the first section's coefficients.
            Section first;

            /// @brief Stores the second section's coefficients.
            Section second;

            /// @brief Stores the first section's magnitude per band, in decibels.
            Bands first_db;

            /// @brief Stores the second section's magnitude per band, in decibels.
            Bands second_db;

            /// @brief Stores the cascade's magnitude minus the target per band, in decibels.
            Bands residual;

            /// @brief Stores the squared residuals plus the weighted squared distance from the
            /// prior.
            double cost;

            /// @brief Stores whether both sections have their poles inside the unit circle.
            bool stable;
        };

        /// @brief Evaluates a parameter vector against the target and the prior.
        Evaluation evaluate(const Parameters& parameters, const Parameters& prior,
                            const Bands& wanted, const BandGrid& grid, double sample_rate)
        {
            Evaluation result{};
            result.first = section_at(parameters, kFirstLogCorner, sample_rate);
            result.second = section_at(parameters, kSecondLogCorner, sample_rate);
            result.first_db = section_db(result.first, grid);
            result.second_db = section_db(result.second, grid);
            result.cost = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                result.residual[band] = parameters[kFlatDb] + result.first_db[band]
                    + result.second_db[band] - wanted[band];
                result.cost += result.residual[band] * result.residual[band];
            }
            for (std::size_t slot = 0; slot < kParameterCount; ++slot)
            {
                const double offset = parameters[slot] - prior[slot];
                result.cost += kPriorWeights[slot] * offset * offset;
            }
            result.stable = is_stable(result.first) && is_stable(result.second);
            return result;
        }

        /// @brief Holds the residuals' derivative with respect to each parameter, one row per
        /// slot.
        using Jacobian = std::array<Bands, kParameterCount>;

        /// @brief Returns the Jacobian at an evaluation: the flat gain's row is one, each
        /// section's four rows are forward differences of that section alone.
        Jacobian jacobian(const Parameters& parameters, const Evaluation& at,
                          const BandGrid& grid, double sample_rate)
        {
            constexpr double delta = 1e-4;
            Jacobian result{};
            result[kFlatDb].fill(1.0);
            for (std::size_t slot = kFirstLogCorner; slot < kParameterCount; ++slot)
            {
                const bool first = slot < kSecondLogCorner;
                const std::size_t start = first ? kFirstLogCorner : kSecondLogCorner;
                Parameters nudged = parameters;
                nudged[slot] += delta;
                const Bands shifted = section_db(section_at(nudged, start, sample_rate), grid);
                const Bands& base = first ? at.first_db : at.second_db;
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
                normal[row][row] += kPriorWeights[row];
                normal[row][row] *= 1.0 + damping;
                double sum = -kPriorWeights[row] * (parameters[row] - prior[row]);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    sum -= slope[row][band] * at.residual[band];
                }
                gradient[row] = sum;
            }
            return solve(normal, gradient, step);
        }

        /// @brief Keeps every corner inside (20 Hz, 0.45 * sample_rate), every Q inside
        /// (0.25, 4) and every peak inside (-30, 30) dB.
        void clamp(Parameters& parameters, double sample_rate) noexcept
        {
            const double lowest = std::log(20.0);
            const double highest = std::log(0.45 * sample_rate);
            const double lowest_q = std::log(0.25);
            const double highest_q = std::log(4.0);
            for (std::size_t first = kFirstLogCorner; first < kParameterCount;
                 first += kSectionSlots)
            {
                parameters[first] = std::clamp(parameters[first], lowest, highest);
                parameters[first + 2] = std::clamp(parameters[first + 2], -30.0, 30.0);
                parameters[first + 3] = std::clamp(parameters[first + 3], lowest_q, highest_q);
            }
        }

        /// @brief Writes a section into the four slots from the given one: the corner, the
        /// step, the peak, Q 0.7071.
        void place(double corner_hz, double step_db, double peak_db, std::size_t first,
                   Parameters& parameters) noexcept
        {
            parameters[first] = std::log(corner_hz);
            parameters[first + 1] = step_db;
            parameters[first + 2] = peak_db;
            parameters[first + 3] = std::log(0.7071);
        }

        /// @brief Returns the closed-form guess: the flat gain at the 125 Hz band; a section at
        /// 250 Hz that steps from there to the mean of the two middle bands and peaks by the
        /// 250 Hz band's departure from the mean of its neighbours; a section at 2 kHz that
        /// steps on to the 4 kHz band and peaks by the 2 kHz band's departure likewise.
        Parameters initial_guess(const Bands& wanted) noexcept
        {
            Parameters parameters{};
            const double middle_db = 0.5 * (wanted[2] + wanted[3]);
            parameters[kFlatDb] = wanted[0];
            place(250.0, middle_db - wanted[0], wanted[1] - 0.5 * (wanted[0] + wanted[2]),
                  kFirstLogCorner, parameters);
            place(2000.0, wanted[5] - middle_db, wanted[4] - 0.5 * (wanted[3] + wanted[5]),
                  kSecondLogCorner, parameters);
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
        return Result{ realise(best.first, best.second, parameters[kFlatDb]),
                       worst(best.residual) };
    }
}
