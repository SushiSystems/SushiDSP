/**************************************************************************/
/* microphone_model.hpp                                                   */
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
 * @file microphone_model.hpp
 * @brief A first-order microphone pattern with its proximity effect, per band.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/room/band_response.hpp"

namespace SushiDSP::room
{
    /// @brief Names the first-order polar patterns the model evaluates. Spec §8.
    enum class PolarPattern : std::uint8_t
    {
        Omni = 0,
        Cardioid,
        Supercardioid,
        FigureEight
    };

    /// @brief Returns the pressure share a of P(theta) = a + (1 - a) cos(theta): 1, 0.5, 0.366,
    /// 0.
    double pattern_pressure_share(PolarPattern pattern) noexcept;

    /// @brief Names each microphone the module ships, in the order of the table in
    /// microphone_model.cpp. A new microphone is one more name here and one more row there.
    enum class MicrophoneKind : std::uint8_t
    {
        FlatCondenser = 0
    };

    /// @brief Counts the shipped microphones.
    inline constexpr std::size_t K_MICROPHONE_KIND_COUNT = 1;

    /// @brief Holds one shipped microphone: what it is called, how it transduces, the pattern it
    /// starts with and its transcribed on-axis curve.
    struct MicrophonePreset
    {
            /// @brief Names the microphone on screen.
            const char* name;

            /// @brief Stores the pattern a microphone of this kind starts with.
            PolarPattern pattern;

            /// @brief Stores the on-axis response per band, in decibels, from the maker's sheet.
            std::array<float, K_BAND_COUNT> on_axis_db;

            /// @brief Names the sheet the curve was transcribed from.
            const char* citation;

            /// @brief Names the microphone as a preset file spells it.
            const char* key;

            /// @brief Names how the capsule transduces: "condenser", "dynamic" or "ribbon".
            const char* transducer;
    };

    /// @brief Returns one shipped microphone; a kind past the table returns the first.
    const MicrophonePreset& microphone_preset(MicrophoneKind kind) noexcept;

    /// @brief Finds the shipped microphone whose key equals the given text exactly.
    /// @param key A null-terminated preset word.
    /// @param kind Written only when a microphone carries the key.
    /// @return true when a microphone carries the key.
    bool find_microphone(const char* key, MicrophoneKind& kind) noexcept;

    /// @brief Evaluates a first-order pattern with its proximity effect, on the control thread.
    /// Spec §8.
    class MicrophoneModel
    {
        public:
            /// @brief Builds the model for one pattern and one on-axis curve.
            /// @param pattern The capsule's polar pattern.
            /// @param on_axis_db The on-axis response per band, in decibels.
            /// @param proximity False leaves the gradient share at its far-field level.
            MicrophoneModel(PolarPattern pattern, const std::array<float, K_BAND_COUNT>& on_axis_db,
                            bool proximity = true) noexcept;

            /// @brief Returns pattern times proximity times the on-axis curve, per band, relative to
            /// the far-field on-axis response.
            /// @param incidence_rad 0 on axis.
            /// @param distance_m Path length from the source to the capsule.
            /// @pre distance_m > 0.
            BandResponse gain_for(double incidence_rad, double distance_m) const noexcept;

            /// @brief Returns sqrt(1 + (c / (2 pi f r))^2), the gradient term's spherical-wave
            /// boost.
            /// @pre distance_m > 0.
            static double proximity_boost(double frequency_hz, double distance_m) noexcept;

        private:
            /// @brief Stores the pressure share a of the pattern.
            double pressure_share_;

            /// @brief Records that the gradient share takes the spherical-wave boost.
            bool proximity_;

            /// @brief Stores the on-axis curve as linear pressure gains.
            BandResponse on_axis_;
    };

    /// @brief Names the flat condenser: an ideal capsule with no curve of its own, which starts
    /// as a cardioid.
    inline constexpr MicrophonePreset K_MICROPHONE_FLAT_CARDIOID{
        "Flat reference", PolarPattern::Cardioid, {0, 0, 0, 0, 0, 0}, "no maker's curve",
        "flat_condenser", "condenser"};
} // namespace SushiDSP::room
