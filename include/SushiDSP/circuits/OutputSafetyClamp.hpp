/****************************************************************/
/* OutputSafetyClamp.hpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file OutputSafetyClamp.hpp
 * @brief The safety rail an amplifier node's output block passes through on its way to the host.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <algorithm>
#include <cstdint>

namespace SushiDSP
{
    /// @brief Scales a block by its node's output gain and holds the result inside the converter's
    /// range, in place.
    /// @param samples The block to scale and clamp.
    /// @param frames How many samples the block holds.
    /// @param gain The output gain applied before the clamp.
    /// @pre samples addresses frames writable floats.
    inline void apply_output_safety_clamp(float* samples, std::uint32_t frames, float gain)
    {
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            samples[i] = std::clamp(samples[i] * gain, -1.0f, 1.0f);
        }
    }
}
