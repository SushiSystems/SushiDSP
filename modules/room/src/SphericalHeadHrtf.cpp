/**************************************************************************/
/* SphericalHeadHrtf.cpp                                                  */
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
 * @file SphericalHeadHrtf.cpp
 * @brief The Duda-Martens series per direction, folded to minimum phase through the cepstrum.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/SphericalHeadHrtf.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "SushiDSP/math/Fft.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Holds the grid step in radians, 10 degrees.
        constexpr double kGridStep = kPi / 18.0;

        /// @brief Counts the bins of the one-sided magnitude grid, 0 to Nyquist inclusive.
        constexpr std::uint32_t kBinCount = 513;

        /// @brief Holds the transform size the cepstrum is folded on.
        constexpr std::uint32_t kFftSize = 2 * (kBinCount - 1);

        /// @brief Counts the series terms taken past the normalised frequency.
        constexpr std::uint32_t kExtraTerms = 40;

        /// @brief Holds one complex value in double precision.
        using Complex = std::complex<double>;

        /// @brief Holds one ear's magnitude on the one-sided grid.
        using Magnitudes = std::array<double, kBinCount>;

        /// @brief Holds one ear's diffuse-field filter.
        using DiffuseTaps = std::array<float, HrtfSet::kDiffuseTaps>;

        static_assert(HrtfSet::kDiffuseTaps == HrtfSet::kTapCount,
                      "the folder writes kTapCount taps and the diffuse filter must hold them");

        /// @brief Returns the far-field rigid-sphere pressure ratio of Duda and Martens at one
        /// normalised frequency mu = k a and one cosine of the angle from the ear.
        Complex sphere_response(double mu, double cos_theta) noexcept
        {
            if (mu <= 0.0)
            {
                return Complex(1.0, 0.0);
            }
            const Complex phase = std::exp(Complex(0.0, mu));
            Complex hankel_previous = phase * Complex(0.0, -1.0 / mu);
            Complex hankel = hankel_previous * Complex(1.0 / mu, -1.0);
            double legendre_previous = 1.0;
            double legendre = cos_theta;
            Complex rotation(0.0, 1.0);
            Complex sum = rotation / (-hankel);
            // Term m is (-i)^(m-1) (2m+1) P_m(cos theta) / h_m'(mu); m = 0 is the line above.
            const auto term_count = static_cast<std::uint32_t>(std::ceil(mu)) + kExtraTerms;
            for (std::uint32_t m = 1; m < term_count; ++m)
            {
                const double order = static_cast<double>(m);
                rotation *= Complex(0.0, -1.0);
                const Complex derivative = hankel_previous - (order + 1.0) / mu * hankel;
                const Complex term = rotation * (2.0 * order + 1.0) * legendre / derivative;
                if (!std::isfinite(term.real()) || !std::isfinite(term.imag()))
                {
                    break;
                }
                sum += term;
                const Complex hankel_next = (2.0 * order + 1.0) / mu * hankel - hankel_previous;
                hankel_previous = hankel;
                hankel = hankel_next;
                const double legendre_next =
                    ((2.0 * order + 1.0) * cos_theta * legendre - order * legendre_previous)
                    / (order + 1.0);
                legendre_previous = legendre;
                legendre = legendre_next;
            }
            return sum / (mu * mu);
        }

        /// @brief Returns the signed lateral angle of a direction: its angle from the median
        /// plane, positive to the left.
        double lateral_angle(double azimuth_rad, double elevation_rad) noexcept
        {
            const double sine =
                std::clamp(std::cos(elevation_rad) * std::sin(azimuth_rad), -1.0, 1.0);
            return std::asin(sine);
        }

        /// @brief Fills one ear's magnitude grid from the cosine of its angle to the source.
        void ear_magnitudes(double cos_theta, double bin_hz, Magnitudes& magnitude) noexcept
        {
            const double mu_per_hz = 2.0 * kPi * SphericalHeadHrtf::kHeadRadiusMetres
                                     / SphericalHeadHrtf::kSpeedOfSound;
            for (std::uint32_t bin = 0; bin < kBinCount; ++bin)
            {
                magnitude[bin] = std::abs(sphere_response(bin * bin_hz * mu_per_hz, cos_theta));
            }
        }

        /// @brief Turns a one-sided magnitude grid into its minimum-phase response by cepstral
        /// folding and keeps the first kTapCount taps.
        class MinimumPhaseFolder
        {
        public:
            /// @brief Sizes the transform and its scratch once.
            MinimumPhaseFolder()
                : fft_(kFftSize), cepstrum_(kFftSize), real_(kFftSize), imag_(kFftSize)
            {
            }

            /// @brief Writes the minimum-phase taps of a magnitude grid into out.
            void fold(const Magnitudes& magnitude, float* out)
            {
                for (std::uint32_t bin = 0; bin < kFftSize; ++bin)
                {
                    const std::uint32_t folded = bin < kBinCount ? bin : kFftSize - bin;
                    real_[bin] = static_cast<float>(std::log(magnitude[folded]));
                    imag_[bin] = 0.0f;
                }
                fft_.inverse(real_.data(), imag_.data(), cepstrum_.data());
                // The real cepstrum of a minimum-phase response is its causal half, doubled.
                for (std::uint32_t n = 1; n < kBinCount - 1; ++n)
                {
                    cepstrum_[n] *= 2.0f;
                    cepstrum_[kFftSize - n] = 0.0f;
                }
                fft_.forward(cepstrum_.data(), real_.data(), imag_.data());
                for (std::uint32_t bin = 0; bin < kFftSize; ++bin)
                {
                    const float gain = std::exp(real_[bin]);
                    const float angle = imag_[bin];
                    real_[bin] = gain * std::cos(angle);
                    imag_[bin] = gain * std::sin(angle);
                }
                fft_.inverse(real_.data(), imag_.data(), cepstrum_.data());
                std::copy(cepstrum_.begin(), cepstrum_.begin() + HrtfSet::kTapCount, out);
            }

        private:
            /// @brief Holds the transform plan.
            Fft fft_;

            /// @brief Holds the real cepstrum, then the time-domain result.
            std::vector<float> cepstrum_;

            /// @brief Holds the real half of the spectra.
            std::vector<float> real_;

            /// @brief Holds the imaginary half of the spectra.
            std::vector<float> imag_;
        };

        /// @brief Writes the azimuth and elevation of one grid index in double precision: 17
        /// rings of 36 azimuths from -170 to 180 degrees, then the lower and the upper pole.
        void grid_point(std::uint32_t index, double& azimuth_rad, double& elevation_rad) noexcept
        {
            constexpr std::uint32_t ring_total =
                SphericalHeadHrtf::kAzimuthSteps * SphericalHeadHrtf::kElevationRings;
            if (index >= ring_total)
            {
                azimuth_rad = 0.0;
                elevation_rad = index == ring_total ? -kPi / 2.0 : kPi / 2.0;
                return;
            }
            const std::uint32_t ring = index / SphericalHeadHrtf::kAzimuthSteps;
            const std::uint32_t step = index % SphericalHeadHrtf::kAzimuthSteps;
            azimuth_rad = (static_cast<double>(step) - 17.0) * kGridStep;
            elevation_rad = (static_cast<double>(ring) - 8.0) * kGridStep;
        }

        /// @brief Returns the solid angle one grid index stands for in steradians, so that an
        /// average weighted by it runs over the sphere and not over the grid's points, which
        /// crowd towards the poles: a ring point takes its ring's band divided by the ring's
        /// count, a pole takes the cap above the top ring's band.
        double solid_angle(std::uint32_t index) noexcept
        {
            constexpr std::uint32_t ring_total =
                SphericalHeadHrtf::kAzimuthSteps * SphericalHeadHrtf::kElevationRings;
            if (index >= ring_total)
            {
                return 2.0 * kPi * (1.0 - std::sin(kPi / 2.0 - kGridStep / 2.0));
            }
            const std::uint32_t ring = index / SphericalHeadHrtf::kAzimuthSteps;
            const double elevation = (static_cast<double>(ring) - 8.0) * kGridStep;
            return 4.0 * kPi * std::cos(elevation) * std::sin(kGridStep / 2.0)
                   / static_cast<double>(SphericalHeadHrtf::kAzimuthSteps);
        }

        /// @brief Accumulates one ear's power over the sphere and renders the average as a
        /// minimum-phase filter. Spec §6.4.
        class DiffuseFieldAverage
        {
        public:
            /// @brief Starts from no power and no weight.
            DiffuseFieldAverage() noexcept : power_{}, weight_(0.0)
            {
            }

            /// @brief Adds one direction's magnitude grid, weighted by its solid angle.
            void add(const Magnitudes& magnitude, double solid_angle) noexcept
            {
                for (std::uint32_t bin = 0; bin < kBinCount; ++bin)
                {
                    power_[bin] += solid_angle * magnitude[bin] * magnitude[bin];
                }
                weight_ += solid_angle;
            }

            /// @brief Writes the root of the mean power, folded to minimum phase, into out.
            /// @pre At least one direction was added.
            void render(MinimumPhaseFolder& folder, DiffuseTaps& out) const
            {
                Magnitudes magnitude{};
                for (std::uint32_t bin = 0; bin < kBinCount; ++bin)
                {
                    magnitude[bin] = std::sqrt(power_[bin] / weight_);
                }
                folder.fold(magnitude, out.data());
            }

        private:
            /// @brief Holds the solid-angle-weighted sum of squared magnitudes per bin.
            std::array<double, kBinCount> power_;

            /// @brief Holds the sum of the solid angles added.
            double weight_;
        };

        /// @brief Returns Woodworth's delay in frames for a lateral angle.
        double woodworth_frames(double theta, double sample_rate) noexcept
        {
            return SphericalHeadHrtf::kHeadRadiusMetres / SphericalHeadHrtf::kSpeedOfSound
                   * (theta + std::sin(theta)) * sample_rate;
        }
    }

    double SphericalHeadHrtf::interaural_time_difference(double azimuth_rad) noexcept
    {
        const double theta = lateral_angle(azimuth_rad, 0.0);
        return kHeadRadiusMetres / kSpeedOfSound * (theta + std::sin(theta));
    }

    HrtfSet SphericalHeadHrtf::build(double sample_rate)
    {
        std::vector<HrtfSet::Direction> directions(kDirectionCount);
        const double bin_hz = sample_rate / static_cast<double>(kFftSize);
        const std::size_t tap_total =
            static_cast<std::size_t>(kDirectionCount) * HrtfSet::kTapCount;
        std::vector<float> left(tap_total);
        std::vector<float> right(tap_total);
        MinimumPhaseFolder folder;
        Magnitudes magnitude{};
        DiffuseFieldAverage diffuse_left;
        DiffuseFieldAverage diffuse_right;
        for (std::uint32_t index = 0; index < kDirectionCount; ++index)
        {
            double azimuth = 0.0;
            double elevation = 0.0;
            grid_point(index, azimuth, elevation);
            const double weight = solid_angle(index);
            const std::size_t offset = static_cast<std::size_t>(index) * HrtfSet::kTapCount;
            // The left ear sits at +90 degrees azimuth and the right at -90, both level.
            const double cos_left = std::cos(elevation) * std::sin(azimuth);
            ear_magnitudes(cos_left, bin_hz, magnitude);
            folder.fold(magnitude, left.data() + offset);
            diffuse_left.add(magnitude, weight);
            ear_magnitudes(-cos_left, bin_hz, magnitude);
            folder.fold(magnitude, right.data() + offset);
            diffuse_right.add(magnitude, weight);
            directions[index] = { static_cast<float>(azimuth), static_cast<float>(elevation),
                                  static_cast<float>(woodworth_frames(
                                      lateral_angle(azimuth, elevation), sample_rate)) };
        }
        DiffuseTaps diffuse_left_taps{};
        DiffuseTaps diffuse_right_taps{};
        diffuse_left.render(folder, diffuse_left_taps);
        diffuse_right.render(folder, diffuse_right_taps);
        HrtfSet set(sample_rate, std::move(directions), std::move(left), std::move(right));
        set.set_diffuse_field(diffuse_left_taps, diffuse_right_taps);
        return set;
    }
}
