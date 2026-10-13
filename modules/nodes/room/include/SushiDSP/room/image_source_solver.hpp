/**************************************************************************/
/* image_source_solver.hpp                                                */
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
 * @file image_source_solver.hpp
 * @brief The image-source solver that turns a shoebox scene into one early tap per image.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>

#include "SushiDSP/room/early_tap_set.hpp"
#include "SushiDSP/room/room_scene.hpp"

namespace SushiDSP::room
{
    /// @brief Names which listener the taps are encoded for. Spec §8.
    enum class ListenerKind : std::uint8_t
    {
        Ears = 0,
        Microphone
    };

    /// @brief Mirrors the source across the six walls to the given order and reduces every
    /// surviving image to one tap with one fitted filter, on the control thread. Spec §4.1.
    class ImageSourceSolver
    {
        public:
            /// @brief Holds what a solve needs beyond the scene.
            struct Options
            {
                    /// @brief Bounds the reflection order; images with more bounces are not enumerated.
                    std::uint32_t max_order = 4;

                    /// @brief Selects the ears' Ambisonic encode or the microphone's pattern.
                    ListenerKind listener = ListenerKind::Ears;

                    /// @brief Stores the sample rate the delays and filters are expressed at, in hertz.
                    double sample_rate = 48000.0;
            };

            /// @brief Returns (2/3) N (N + 1) (2N + 1) + 2N + 1, the direct sound included.
            static std::uint32_t image_count(std::uint32_t order) noexcept;

            /// @brief Returns (order + 1) min(L_x, L_y, L_z) / c, after which the image set is
            /// incomplete.
            static double crossover_time_s(const RoomScene& scene, std::uint32_t order) noexcept;

            /// @brief Returns the whole tap set, the line of sight first and the reflections after
            /// it sorted by arrival, with nothing past crossover_time_s and never more than
            /// EarlyTapSet::K_MAX_EARLY_TAPS entries. A microphone's line of sight is one tap per
            /// radiating element of the cabinet; the ears' is one tap.
            /// @pre scene.is_valid() holds and options.sample_rate exceeds 8 kHz.
            static EarlyTapSet solve(const RoomScene& scene, const Options& options);
    };
} // namespace SushiDSP::room
