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
 * @file output_safety_clamp.hpp
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
} // namespace SushiDSP
