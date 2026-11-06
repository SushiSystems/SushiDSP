/**************************************************************************/
/* output_safety_clamp.hpp                                                */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file output_safety_clamp.hpp
 * @brief The safety rail an amplifier node's output block passes through on its way to the host.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace SushiDSP
{
    /// @brief Scales a block by its node's output gain and holds the result inside the converter's
    /// range, in place.
    /// @param samples The block to scale and clamp.
    /// @param frames How many samples the block holds.
    /// @param gain The output gain applied before the clamp.
    /// @return How many samples were clipped to the rail or were not a number; a NaN is counted
    /// and passes through unchanged.
    /// @pre samples addresses frames writable floats.
    [[nodiscard]] inline std::uint32_t apply_output_safety_clamp(float* samples,
                                                                 std::uint32_t frames, float gain)
    {
        std::uint32_t cut = 0;
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            const float scaled = samples[i] * gain;
            cut += static_cast<std::uint32_t>(std::isnan(scaled) | (scaled > 1.0f) |
                                              (scaled < -1.0f));
            samples[i] = std::clamp(scaled, -1.0f, 1.0f);
        }
        return cut;
    }
} // namespace SushiDSP
