/**************************************************************************/
/* MicrophoneModel.cpp                                                    */
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
 * @file MicrophoneModel.cpp
 * @brief The pattern, proximity and on-axis factors of the microphone model.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/MicrophoneModel.hpp"

#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double kPi = 3.14159265358979323846;

        /// @brief Holds the pressure share of each PolarPattern in enumeration order.
        constexpr std::array<double, 4> kPressureShare = { 1.0, 0.5, 0.366, 0.0 };
    }

    double pattern_pressure_share(PolarPattern pattern) noexcept
    {
        return kPressureShare[static_cast<std::size_t>(pattern)];
    }

    MicrophoneModel::MicrophoneModel(PolarPattern pattern,
                                     const std::array<float, kBandCount>& on_axis_db) noexcept
        : pressure_share_(pattern_pressure_share(pattern))
        , on_axis_(BandResponse::from_decibels(on_axis_db))
    {
    }

    double MicrophoneModel::proximity_boost(double frequency_hz, double distance_m) noexcept
    {
        const double ratio = kSpeedOfSound / (2.0 * kPi * frequency_hz * distance_m);
        return std::sqrt(1.0 + ratio * ratio);
    }

    BandResponse MicrophoneModel::gain_for(double incidence_rad, double distance_m) const noexcept
    {
        // The boost multiplies the gradient share (1 - a) cos(theta) and never the pressure
        // share a, so the on-axis far field reads a + (1 - a) = 1 and a 5 cm cardioid reads
        // the 15.5 dB of spec section 8.1 at 100 Hz.
        const double gradient = (1.0 - pressure_share_) * std::cos(incidence_rad);
        BandResponse response{};
        for (std::size_t band = 0; band < kBandCount; ++band)
        {
            const double boost = proximity_boost(kBandCentresHz[band], distance_m);
            const double pattern = pressure_share_ + gradient * boost;
            response.gain[band] = static_cast<float>(
                std::fabs(pattern) * static_cast<double>(on_axis_.gain[band]));
        }
        return response;
    }
}
