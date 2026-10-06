/**************************************************************************/
/* FadeWindow.hpp                                                         */
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
 * @file FadeWindow.hpp
 * @brief The crossfade window every layer of the room blends under, and the ears they render into.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cmath>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Counts the frames a newly acquired set fades in over, raised cosine.
    inline constexpr std::uint32_t kCrossfadeFrames = 512;

    /// @brief Counts the ears the room's layers are rendered for.
    inline constexpr std::uint32_t kEars = 2;

    /// @brief Holds the frames of one block a crossfade covers, how far the fade had run
    /// when the block began, and the lag the fade is read at by a path that blends at the
    /// output: one partition when the ears take part, so the paths the decoder or the
    /// alignment delay hold back, which read the window unlagged, come out in step.
    struct FadeWindow
    {
        /// @brief Counts the frames that ran since the set was adopted, before this block.
        std::uint32_t done;

        /// @brief Counts the frames of this block still inside the fade.
        std::uint32_t fading;

        /// @brief Stores the frames a path blending at the output reads the fade behind.
        std::uint32_t lag;
    };

    /// @brief Returns the arriving set's raised-cosine weight at one frame of the fade, one
    /// past its end: a path that runs ahead of the fade by its own latency reads past it.
    inline float fade_weight(std::uint32_t frame, std::uint32_t total) noexcept
    {
        if (frame + 1u >= total)
        {
            return 1.0f;
        }
        constexpr double kPi = 3.14159265358979323846;
        const double phase = static_cast<double>(frame + 1u) / static_cast<double>(total);
        return static_cast<float>(0.5 * (1.0 - std::cos(kPi * phase)));
    }

    /// @brief Returns the arriving set's weight at one frame of a window: read at the window's
    /// lag by a path that blends at the output, unlagged by a path whose output waits that lag
    /// downstream.
    inline float window_weight(const FadeWindow& window, std::uint32_t frame,
                               bool lagged) noexcept
    {
        const std::uint32_t index = window.done + frame;
        if (!lagged)
        {
            return fade_weight(index, kCrossfadeFrames);
        }
        return index < window.lag ? 0.0f : fade_weight(index - window.lag, kCrossfadeFrames);
    }

    /// @brief Blends a retiring pair of buffers into an arriving pair under the fade.
    void blend_pair(float* __restrict left, float* __restrict right,
                    const float* __restrict old_left, const float* __restrict old_right,
                    const FadeWindow& window, bool lagged) noexcept;
}
