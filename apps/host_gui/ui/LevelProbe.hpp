/**************************************************************************/
/* LevelProbe.hpp                                                         */
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

#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace ui
{
    /// @brief Carries a block's peak magnitude from the audio thread to the GUI thread.
    class LevelProbe
    {
    public:
        /// @brief Raises the stored peak to this block's largest magnitude. Audio thread only:
        /// allocation-free, lock-free, and never blocks.
        /// @param samples The block; may be nullptr when count is 0.
        /// @param count Frames in the block.
        void report(const float* samples, std::uint32_t count) noexcept
        {
            float block_peak = 0.0f;
            for (std::uint32_t frame = 0; frame < count; ++frame)
            {
                block_peak = std::fmax(block_peak, std::fabs(samples[frame]));
            }

            float seen = peak_.load(std::memory_order_relaxed);
            while (block_peak > seen
                && !peak_.compare_exchange_weak(seen, block_peak, std::memory_order_relaxed))
            {
            }
        }

        /// @brief Reads the largest peak reported since the previous call and resets the
        /// accumulator. GUI thread only.
        /// @return The peak magnitude.
        float read_peak() noexcept
        {
            return peak_.exchange(0.0f, std::memory_order_relaxed);
        }

    private:
        std::atomic<float> peak_{ 0.0f };
    };
}
