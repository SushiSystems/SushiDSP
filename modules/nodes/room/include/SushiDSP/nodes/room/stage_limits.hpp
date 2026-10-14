/**************************************************************************/
/* stage_limits.hpp                                                       */
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
 * @file stage_limits.hpp
 * @brief The bounds the room's stages size their rings from.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cmath>
#include <cstdint>

#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

namespace SushiDSP::Room
{
    /// @brief Holds the reflection order every pair's set is solved to.
    inline constexpr std::uint32_t K_IMAGE_ORDER = 4;

    /// @brief Holds the frames the direct path's ring keeps past the longest tap for the far
    /// ear's ITD, 2.7 ms at 48 kHz against Woodworth's 0.66 ms.
    inline constexpr std::uint32_t K_ITD_MARGIN_FRAMES = 128;

    /// @brief Returns the frames the longest crossover time the scene bounds allow takes at one
    /// rate: (order + 1) * K_MAX_DIMENSION / c, 8397 at 48 kHz.
    /// @param sample_rate The rate in hertz.
    /// @return The frame count, rounded up.
    inline std::uint32_t max_early_delay_frames(double sample_rate) noexcept
    {
        const double seconds = static_cast<double>(K_IMAGE_ORDER + 1u) *
                               room::RoomScene::K_MAX_DIMENSION / room::K_SPEED_OF_SOUND;
        return static_cast<std::uint32_t>(std::ceil(seconds * sample_rate));
    }
} // namespace SushiDSP::Room
