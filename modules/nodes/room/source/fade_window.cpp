/**************************************************************************/
/* fade_window.cpp                                                        */
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
 * @file fade_window.cpp
 * @brief The blend one crossfade window applies to a pair of buffers.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/fade_window.hpp>

namespace SushiDSP::room
{
    void blend_pair(float* const __restrict left, float* const __restrict right,
                    const float* const __restrict old_left, const float* const __restrict old_right,
                    const FadeWindow& window, bool lagged) noexcept
    {
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, lagged);
            left[frame] = old_left[frame] + weight * (left[frame] - old_left[frame]);
            right[frame] = old_right[frame] + weight * (right[frame] - old_right[frame]);
        }
    }
} // namespace SushiDSP::room
