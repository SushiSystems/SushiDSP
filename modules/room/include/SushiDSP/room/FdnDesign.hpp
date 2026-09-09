/**************************************************************************/
/* FdnDesign.hpp                                                          */
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
 * @file FdnDesign.hpp
 * @brief The feedback delay network's coefficients as the control thread publishes them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace SushiDSP::room
{
    /// @brief Holds the late tail's line lengths, per-line filters and output shaping. Spec §6.
    struct FdnDesign
    {
        /// @brief Counts the delay lines.
        static constexpr std::size_t kLineCount = 16;

        /// @brief Counts the biquad sections each output filter cascades.
        static constexpr std::size_t kOutputSections = 2;

        /// @brief Holds one biquad section's b0, b1, b2, a1, a2.
        using Section = std::array<double, 5>;

        /// @brief Holds one fourth-order output filter as its two sections, run in order.
        using Cascade = std::array<Section, kOutputSections>;

        /// @brief Stores each line's length in frames.
        std::array<std::uint32_t, kLineCount> line_frames;

        /// @brief Stores b0, b1, b2, a1, a2 of each line's first absorption section.
        std::array<std::array<double, 5>, kLineCount> line_section_a;

        /// @brief Stores b0, b1, b2, a1, a2 of each line's second absorption section.
        std::array<std::array<double, 5>, kLineCount> line_section_b;

        /// @brief Stores the tone-correction cascade applied to each ear's summed output.
        Cascade tone_correction;

        /// @brief Stores the coherence filter gamma of spec §6.4.
        Cascade coherence_a;

        /// @brief Stores the sqrt(1 - gamma^2) filter of spec §6.4.
        Cascade coherence_b;

        /// @brief Stores the linear gain into the network.
        float input_gain;

        /// @brief Stores the delay in frames from the input to the crossover time t_c.
        float delay_to_crossover_frames;
    };
}
