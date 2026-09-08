/**************************************************************************/
/* BandResponse.hpp                                                       */
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
 * @file BandResponse.hpp
 * @brief The six-band gain vector every quantity in the room module is expressed in.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace SushiDSP::room
{
    /// @brief Counts the octave bands every band quantity in this module is sampled at.
    inline constexpr std::size_t kBandCount = 6;

    /// @brief Names the six ISO 354 octave centres, in hertz. Spec §4.2.
    inline constexpr std::array<double, kBandCount> kBandCentresHz =
        { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0 };

    /// @brief Gives the speed of sound in dry air at 20 degrees Celsius, in metres per second.
    /// Spec §4.2.
    inline constexpr double kSpeedOfSound = 343.0;

    /// @brief Bounds how far below unity a band gain may read in decibels.
    inline constexpr float kDecibelFloor = -200.0f;

    /// @brief Holds a pressure gain per octave band, linear, never in decibels on the wire.
    struct BandResponse
    {
        /// @brief Stores the linear pressure gain of each band, ordered as kBandCentresHz.
        std::array<float, kBandCount> gain;

        /// @brief Builds the response that passes every band unchanged.
        static BandResponse unity() noexcept
        {
            BandResponse response{};
            response.gain.fill(1.0f);
            return response;
        }

        /// @brief Converts a gain per band given in decibels into linear pressure gains.
        static BandResponse from_decibels(const std::array<float, kBandCount>& decibels) noexcept
        {
            BandResponse response{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                response.gain[band] =
                    static_cast<float>(std::pow(10.0, static_cast<double>(decibels[band]) / 20.0));
            }
            return response;
        }

        /// @brief Multiplies this response by another band by band.
        BandResponse& operator*=(const BandResponse& other) noexcept
        {
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                gain[band] *= other.gain[band];
            }
            return *this;
        }

        /// @brief Raises every band gain to the given exponent.
        BandResponse powered(double exponent) const noexcept
        {
            BandResponse response{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                response.gain[band] =
                    static_cast<float>(std::pow(static_cast<double>(gain[band]), exponent));
            }
            return response;
        }

        /// @brief Returns the power mean over the six bands, which is what a diffuse field
        /// receives.
        float power_mean() const noexcept
        {
            double sum_of_squares = 0.0;
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double band_gain = static_cast<double>(gain[band]);
                sum_of_squares += band_gain * band_gain;
            }
            return static_cast<float>(
                std::sqrt(sum_of_squares / static_cast<double>(kBandCount)));
        }

        /// @brief Returns the gain of each band in decibels.
        /// @pre A gain at or below 10^(-10) reads as kDecibelFloor, so the conversion is total.
        std::array<float, kBandCount> decibels() const noexcept
        {
            constexpr double floor_gain = 1.0e-10;
            std::array<float, kBandCount> result{};
            for (std::size_t band = 0; band < kBandCount; ++band)
            {
                const double band_gain = static_cast<double>(gain[band]);
                result[band] = band_gain > floor_gain
                    ? static_cast<float>(20.0 * std::log10(band_gain))
                    : kDecibelFloor;
            }
            return result;
        }
    };
}
