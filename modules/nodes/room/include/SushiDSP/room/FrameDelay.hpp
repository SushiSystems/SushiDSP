/**************************************************************************/
/* FrameDelay.hpp                                                         */
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
