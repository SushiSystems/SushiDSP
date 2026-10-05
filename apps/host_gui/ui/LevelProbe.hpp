/****************************************************************/
/* LevelProbe.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file LevelProbe.hpp
 * @brief Defines the probe that carries a block's peak from the audio thread to the GUI.
 * @author Mustafa Garip
 */

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
        static_assert(std::atomic<float>::is_always_lock_free,
                      "LevelProbe::report runs on the audio thread, which must never block");

        std::atomic<float> peak_{ 0.0f };
    };
}
