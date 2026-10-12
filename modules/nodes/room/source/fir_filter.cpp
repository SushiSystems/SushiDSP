/**************************************************************************/
/* fir_filter.cpp                                                         */
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
 * @file fir_filter.cpp
 * @brief The reversed-tap inner product one block of the convolution runs.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/fir_filter.hpp"

#include <algorithm>

namespace SushiDSP::room
{
    void FirFilter::prepare(std::uint32_t max_block_frames)
    {
        history_.assign(static_cast<std::size_t>(K_TAPS) - 1u + max_block_frames, 0.0f);
    }

    void FirFilter::load(const float* taps) noexcept
    {
        // The taps are kept reversed, so the inner product runs forward over the history.
        for (std::uint32_t index = 0; index < K_TAPS; ++index)
        {
            taps_[index] = taps[K_TAPS - 1u - index];
        }
    }

    void FirFilter::reset() noexcept
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
    }

    void FirFilter::carry_from(const FirFilter& other) noexcept
    {
        std::copy(other.history_.begin(), other.history_.end(), history_.begin());
    }

    void FirFilter::process(const float* input, float* output, std::uint32_t frames) noexcept
    {
        float* const history = history_.data();
        const float* const __restrict taps = taps_.data();
        std::copy_n(input, frames, history + (K_TAPS - 1u));
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const float* const __restrict window = history + frame;
            float sum = 0.0f;
            for (std::uint32_t index = 0; index < K_TAPS; ++index)
            {
                sum += taps[index] * window[index];
            }
            output[frame] = sum;
        }
        std::copy(history + frames, history + frames + (K_TAPS - 1u), history);
    }
} // namespace SushiDSP::room
