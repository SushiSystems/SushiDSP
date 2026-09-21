/**************************************************************************/
/* FrameDelay.hpp                                                         */
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
 * @file FrameDelay.hpp
 * @brief The whole-frame delay line one layer waits at the decoder's partition through.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP::room
{
    /// @brief Delays a block by a fixed count of frames. Audio thread.
    class FrameDelay
    {
    public:
        /// @brief Sizes the line for the delay and the longest block.
        void prepare(std::uint32_t delay_frames, std::uint32_t max_block_frames);

        /// @brief Zeroes the line.
        void reset() noexcept;

        /// @brief Writes one block delayed by the prepared count into output.
        /// @pre frames <= max_block_frames; input and output may alias.
        void process(const float* input, float* output, std::uint32_t frames) noexcept;

    private:
        /// @brief Stores the delayed frames followed by room for one block.
        std::vector<float> line_;

        /// @brief Stores the prepared delay.
        std::uint32_t delay_frames_ = 0;
    };
}
