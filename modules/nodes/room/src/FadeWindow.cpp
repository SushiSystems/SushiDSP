/**************************************************************************/
/* FadeWindow.cpp                                                         */
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
