/**************************************************************************/
/* microphone_model.cpp                                                   */
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
 * @file microphone_model.cpp
 * @brief The pattern, proximity and on-axis factors of the microphone model.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/microphone_model.hpp>

#include <cmath>
#include <cstring>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Holds the shipped microphones, ordered as MicrophoneKind.
        constexpr std::array<MicrophonePreset, K_MICROPHONE_KIND_COUNT> K_PRESETS = {
            K_MICROPHONE_FLAT_CARDIOID};

        /// @brief Holds the pressure share of each PolarPattern in enumeration order.
        constexpr std::array<double, 4> K_PRESSURE_SHARE = {1.0, 0.5, 0.366, 0.0};
    } // namespace

    const MicrophonePreset& microphone_preset(MicrophoneKind kind) noexcept
    {
        const auto index = static_cast<std::size_t>(kind);
        return K_PRESETS[index < K_MICROPHONE_KIND_COUNT ? index : 0u];
    }

    bool find_microphone(const char* key, MicrophoneKind& kind) noexcept
    {
        for (std::size_t index = 0; index < K_MICROPHONE_KIND_COUNT; ++index)
        {
            if (std::strcmp(K_PRESETS[index].key, key) == 0)
            {
                kind = static_cast<MicrophoneKind>(index);
                return true;
            }
        }
        return false;
    }

    double pattern_pressure_share(PolarPattern pattern) noexcept
    {
        return K_PRESSURE_SHARE[static_cast<std::size_t>(pattern)];
    }

    MicrophoneModel::MicrophoneModel(PolarPattern pattern,
                                     const std::array<float, K_BAND_COUNT>& on_axis_db,
                                     bool proximity) noexcept
        : pressure_share_(pattern_pressure_share(pattern)), proximity_(proximity),
          on_axis_(BandResponse::from_decibels(on_axis_db))
    {
    }

    double MicrophoneModel::proximity_boost(double frequency_hz, double distance_m) noexcept
    {
        const double ratio = K_SPEED_OF_SOUND / (2.0 * K_PI * frequency_hz * distance_m);
        return std::sqrt(1.0 + ratio * ratio);
    }

    BandResponse MicrophoneModel::gain_for(double incidence_rad, double distance_m) const noexcept
    {
        // The boost scales only the gradient share; see modules/nodes/room/source/README.md.
        const double gradient = (1.0 - pressure_share_) * std::cos(incidence_rad);
        BandResponse response{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            const double boost =
                proximity_ ? proximity_boost(K_BAND_CENTRES_HZ[band], distance_m) : 1.0;
            const double pattern = pressure_share_ + gradient * boost;
            response.gain[band] =
                static_cast<float>(std::fabs(pattern) * static_cast<double>(on_axis_.gain[band]));
        }
        return response;
    }
} // namespace SushiDSP::room
