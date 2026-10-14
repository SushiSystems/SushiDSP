/**************************************************************************/
/* delay_ring.cpp                                                         */
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
 * @file delay_ring.cpp
 * @brief The third-order Lagrange read one fractional delay comes out of the ring at.
 * @author Mustafa Garip & Sushi Systems
 */

#include <SushiDSP/nodes/room/delay_ring.hpp>

#include <algorithm>
#include <cmath>

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Returns the smallest power of two at or above a count.
        std::uint32_t next_power_of_two(std::uint32_t count) noexcept
        {
            std::uint32_t power = 1;
            while (power < count)
            {
                power <<= 1u;
            }
            return power;
        }

        /// @brief Holds the four Lagrange weights of one read, already scaled by its gain.
        struct LagrangeKernel
        {
                /// @brief Stores the weight of the sample two frames behind the integer delay.
                float c0;

                /// @brief Stores the weight of the sample one frame behind the integer delay.
                float c1;

                /// @brief Stores the weight of the sample at the integer delay.
                float c2;

                /// @brief Stores the weight of the sample one frame ahead of the integer delay.
                float c3;
        };

        /// @brief Returns the third-order Lagrange weights the early renderer reads with, for
        /// a read at fraction frac behind the integer delay, scaled by gain.
        LagrangeKernel lagrange_kernel(double frac, double gain) noexcept
        {
            const double s = 2.0 - frac;
            const double s1 = s - 1.0;
            const double s2 = s - 2.0;
            const double s3 = s - 3.0;
            return LagrangeKernel{static_cast<float>(-s1 * s2 * s3 * gain / 6.0),
                                  static_cast<float>(s * s2 * s3 * gain / 2.0),
                                  static_cast<float>(-s * s1 * s3 * gain / 2.0),
                                  static_cast<float>(s * s1 * s2 * gain / 6.0)};
        }
    } // namespace

    void DelayRing::prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames)
    {
        max_delay_frames_ = max_delay_frames;
        const std::uint32_t capacity = next_power_of_two(max_delay_frames + max_block_frames + 4u);
        ring_.assign(capacity, 0.0f);
        mask_ = capacity - 1u;
        block_start_ = 0;
        write_head_ = 0;
    }

    void DelayRing::reset() noexcept
    {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        block_start_ = 0;
        write_head_ = 0;
    }

    void DelayRing::write(const float* input, std::uint32_t frames) noexcept
    {
        float* const ring = ring_.data();
        block_start_ = write_head_;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            ring[(write_head_ + frame) & mask_] = input[frame];
        }
        write_head_ = (write_head_ + frames) & mask_;
    }

    void DelayRing::read(double delay_frames, float gain, float* output,
                         std::uint32_t frames) const noexcept
    {
        const double delay =
            std::min(std::max(delay_frames, 1.0), static_cast<double>(max_delay_frames_));
        const double whole = std::floor(delay);
        const LagrangeKernel kernel = lagrange_kernel(delay - whole, static_cast<double>(gain));
        // The window starts two frames behind the integer delay, as the early renderer's does.
        const std::uint32_t start = (block_start_ + static_cast<std::uint32_t>(ring_.size()) -
                                     static_cast<std::uint32_t>(whole) - 2u) &
                                    mask_;
        const float* const ring = ring_.data();
        const std::uint32_t mask = mask_;
        // The frames whose four-sample window ends inside the ring are read without the mask.
        const std::uint32_t size = static_cast<std::uint32_t>(ring_.size());
        const std::uint32_t straight = start + 3u < size ? std::min(frames, size - 3u - start) : 0u;
        const float* const __restrict source = ring + start;
        float* const __restrict destination = output;
        const float c0 = kernel.c0;
        const float c1 = kernel.c1;
        const float c2 = kernel.c2;
        const float c3 = kernel.c3;
        for (std::uint32_t frame = 0; frame < straight; ++frame)
        {
            destination[frame] = c0 * source[frame] + c1 * source[frame + 1u] +
                                 c2 * source[frame + 2u] + c3 * source[frame + 3u];
        }
        for (std::uint32_t frame = straight; frame < frames; ++frame)
        {
            const std::uint32_t index = start + frame;
            output[frame] = c0 * ring[index & mask] + c1 * ring[(index + 1u) & mask] +
                            c2 * ring[(index + 2u) & mask] + c3 * ring[(index + 3u) & mask];
        }
    }
} // namespace SushiDSP::room
