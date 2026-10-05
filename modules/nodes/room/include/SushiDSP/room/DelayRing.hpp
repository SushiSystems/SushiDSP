/****************************************************************/
/* DelayRing.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DelayRing.hpp
 * @brief The geometric half's own ring, read at a fraction of a frame.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP::room
{
    /// @brief Reads fractional delays out of one input ring. Audio thread.
    class DelayRing
    {
    public:
        /// @brief Sizes the ring for the longest read and the longest block.
        void prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames);

        /// @brief Zeroes the ring.
        void reset() noexcept;

        /// @brief Writes one block. Call once per block, before any read.
        void write(const float* input, std::uint32_t frames) noexcept;

        /// @brief Reads one block at a fractional delay behind the block last written,
        /// through the early renderer's four-point Lagrange kernel, scaled by gain.
        /// @pre frames matches the block last written; delay_frames is clamped to
        /// [1, max_delay_frames].
        void read(double delay_frames, float gain, float* output,
                  std::uint32_t frames) const noexcept;

    private:
        /// @brief Stores the ring, a power of two long.
        std::vector<float> ring_;

        /// @brief Masks an index into the ring.
        std::uint32_t mask_ = 0;

        /// @brief Indexes the ring frame the last written block started at.
        std::uint32_t block_start_ = 0;

        /// @brief Indexes the ring frame the next write lands on.
        std::uint32_t write_head_ = 0;

        /// @brief Bounds the delay a read may ask for.
        std::uint32_t max_delay_frames_ = 0;
    };
}
