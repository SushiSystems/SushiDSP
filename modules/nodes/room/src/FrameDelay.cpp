/**************************************************************************/
/* FrameDelay.cpp                                                         */
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
 * @file FrameDelay.cpp
 * @brief The copy one block through a whole-frame line costs.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/FrameDelay.hpp"

#include <algorithm>

namespace SushiDSP::room
{
    void FrameDelay::prepare(std::uint32_t delay_frames, std::uint32_t max_block_frames)
    {
        delay_frames_ = delay_frames;
        line_.assign(static_cast<std::size_t>(delay_frames) + max_block_frames, 0.0f);
    }

    void FrameDelay::reset() noexcept
    {
        std::fill(line_.begin(), line_.end(), 0.0f);
    }

    void FrameDelay::process(const float* input, float* output, std::uint32_t frames) noexcept
    {
        float* const line = line_.data();
        std::copy_n(input, frames, line + delay_frames_);
        std::copy_n(line, frames, output);
        std::copy(line + frames, line + frames + delay_frames_, line);
    }
}
