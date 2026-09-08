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
 * @brief A two-shelf closed-form guess refined by damped Gauss-Newton steps.
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

        /// @brief Bounds the refinement at eight steps.
        constexpr int kMaxSteps = 8;

        /// @brief Stops the refinement once the worst band error is under this, in decibels.
        constexpr double kGoodEnoughDb = 0.25;

        /// @brief Holds the parameter vector the refinement moves.
        using Parameters = std::array<double, kParameterCount>;

        /// @brief Holds one section's five normalised coefficients.
        using Section = std::array<double, 5>;

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

        /// @brief Builds the cascade a parameter vector describes, the flat gain folded into the
        /// first section's numerator.
        BandResponseFit::Coefficients realise(const Parameters& parameters, double sample_rate)
        {
            BandResponseFit::Coefficients coefficients{};
            coefficients.section_a = shelf(sample_rate, std::exp(parameters[kLowLogCorner]),
                                           parameters[kLowDb], std::exp(parameters[kLowLogQ]),
                                           true);
            coefficients.section_b = shelf(sample_rate, std::exp(parameters[kHighLogCorner]),
                                           parameters[kHighDb], std::exp(parameters[kHighLogQ]),
                                           false);
            const double flat = std::pow(10.0, parameters[kFlatDb] / 20.0);
            for (std::size_t index = 0; index < 3; ++index)
            {
                coefficients.section_a[index] *= flat;
            }
            return coefficients;
        }

        /// @brief Returns true when a section's poles lie inside the unit circle.
        bool is_stable(const Section& section) noexcept
        {
            const double a1 = section[3];
            const double a2 = section[4];
            return std::fabs(a2) < 1.0 && std::fabs(a1) < 1.0 + a2;
        }

        /// @brief Returns true when both sections are stable.
        bool is_stable(const BandResponseFit::Coefficients& coefficients) noexcept
        {
            return is_stable(coefficients.section_a) && is_stable(coefficients.section_b);
        }

        /// @brief Returns the target's six log-magnitudes in decibels, floored at kDecibelFloor.
        std::array<double, kBandCount> target_db(const BandResponse& target) noexcept
        {
            std::array<double, kBandCount> result{};
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
            std::array<double, kBandCount> cosine;

            /// @brief Stores cos(2w) per band.
            std::array<double, kBandCount> double_cosine;
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

        /// @brief Returns the cascade's squared magnitude at one grid point.
        double power_gain(const BandResponseFit::Coefficients& coefficients, double cosine,
                          double double_cosine) noexcept
        {
            double power = 1.0;
            for (const Section* section : { &coefficients.section_a, &coefficients.section_b })
            {
                const double numerator = squared_magnitude((*section)[0], (*section)[1],
                                                           (*section)[2], cosine, double_cosine);
                const double denominator = squared_magnitude(1.0, (*section)[3], (*section)[4],
                                                             cosine, double_cosine);
                power *= numerator / denominator;
            }
            return power;
        }

        /// @brief Returns the six residuals, model minus target, in decibels.
        std::array<double, kBandCount> residuals(const BandResponseFit::Coefficients& coefficients,
                                                 const std::array<double, kBandCount>& wanted,
                                                 const BandGrid& grid) noexcept
        {
            std::array<double, kBandCount> result{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double power =
                    power_gain(coefficients, grid.cosine[band], grid.double_cosine[band]);
                result[band] = 10.0 * std::log10(std::max(power, 1e-20)) - wanted[band];
            }
            return result;
        }

        /// @brief Returns the largest absolute residual.
        double worst(const std::array<double, kBandCount>& residual) noexcept
        {
            double largest = 0.0;
            for (const double value : residual)
            {
                largest = std::max(largest, std::fabs(value));
            }
            return largest;
        }

        /// @brief Solves the symmetric positive-definite system normal * step = right_hand_side by
        /// Gaussian elimination with partial pivoting; returns false when it is singular.
        bool solve(std::array<std::array<double, kParameterCount>, kParameterCount> normal,
                   std::array<double, kParameterCount> right_hand_side,
                   std::array<double, kParameterCount>& step) noexcept
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
        Parameters initial_guess(const std::array<double, kBandCount>& wanted) noexcept
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
        const double power = power_gain(coefficients, std::cos(omega), std::cos(2.0 * omega));
        return 10.0 * std::log10(std::max(power, 1e-20));
    }

    BandResponseFit::Result BandResponseFit::fit(const BandResponse& target, double sample_rate)
    {
        const std::array<double, kBandCount> wanted = target_db(target);
        Parameters parameters = initial_guess(wanted);
        clamp(parameters, sample_rate);
        const BandGrid grid = band_grid(sample_rate);
        Coefficients best = realise(parameters, sample_rate);
        std::array<double, kBandCount> residual = residuals(best, wanted, grid);
        double best_error = worst(residual);
        double damping = 1e-3;
        for (int step_index = 0; step_index < kMaxSteps && best_error >= kGoodEnoughDb; ++step_index)
        {
            std::array<std::array<double, kBandCount>, kParameterCount> jacobian{};
            for (std::size_t slot = 0; slot < kParameterCount; ++slot)
            {
                const double delta = 1e-4;
                Parameters nudged = parameters;
                nudged[slot] += delta;
                const std::array<double, kBandCount> shifted =
                    residuals(realise(nudged, sample_rate), wanted, grid);
                for (std::size_t band = 0; band < kBandCount; ++band)
                {
                    jacobian[slot][band] = (shifted[band] - residual[band]) / delta;
                }
            }
            bool accepted = false;
            for (int attempt = 0; attempt < 6 && !accepted; ++attempt)
            {
                std::array<std::array<double, kParameterCount>, kParameterCount> normal{};
                std::array<double, kParameterCount> gradient{};
                for (std::size_t row = 0; row < kParameterCount; ++row)
                {
                    for (std::size_t column = 0; column < kParameterCount; ++column)
                    {
                        double sum = 0.0;
                        for (std::size_t band = 0; band < kBandCount; ++band)
                        {
                            sum += jacobian[row][band] * jacobian[column][band];
                        }
                        normal[row][column] = sum;
                    }
                    normal[row][row] *= 1.0 + damping;
                    normal[row][row] += damping;
                    double sum = 0.0;
                    for (std::size_t band = 0; band < kBandCount; ++band)
                    {
                        sum -= jacobian[row][band] * residual[band];
                    }
                    gradient[row] = sum;
                }
                std::array<double, kParameterCount> step{};
                if (!solve(normal, gradient, step))
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
                const Coefficients trial = realise(candidate, sample_rate);
                const std::array<double, kBandCount> trial_residual =
                    residuals(trial, wanted, grid);
                const double trial_error = worst(trial_residual);
                if (is_stable(trial) && trial_error < best_error)
                {
                    parameters = candidate;
                    best = trial;
                    residual = trial_residual;
                    best_error = trial_error;
                    damping = std::max(damping * 0.1, 1e-6);
                    accepted = true;
                }
                else
                {
                    damping *= 10.0;
                }
            }
            if (!accepted)
            {
                break;
            }
        }
        return Result{ best, best_error };
    }
}
