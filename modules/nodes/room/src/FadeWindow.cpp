/****************************************************************/
/* FadeWindow.cpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FadeWindow.cpp
 * @brief The blend one crossfade window applies to a pair of buffers.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/FadeWindow.hpp"

namespace SushiDSP::room
{
    void blend_pair(float* const __restrict left, float* const __restrict right,
                    const float* const __restrict old_left,
                    const float* const __restrict old_right, const FadeWindow& window,
                    bool lagged) noexcept
    {
        for (std::uint32_t frame = 0; frame < window.fading; ++frame)
        {
            const float weight = window_weight(window, frame, lagged);
            left[frame] = old_left[frame] + weight * (left[frame] - old_left[frame]);
            right[frame] = old_right[frame] + weight * (right[frame] - old_right[frame]);
        }
    }
}
