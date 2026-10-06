/**************************************************************************/
/* EarlyReflectionRenderer.hpp                                            */
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
 * @file EarlyReflectionRenderer.hpp
 * @brief The audio-thread renderer that reads every early tap out of one input ring.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/EarlyTapSet.hpp"

namespace SushiDSP::room
{
    /// @brief Reads up to 128 fractional taps out of one input ring, filters each through two
    /// biquads from one of two state banks and encodes it into sixteen channels. Audio thread:
    /// allocation-free after prepare(). Spec §5.1.
    class EarlyReflectionRenderer
    {
    public:
        /// @brief Counts the accumulation buffers render() adds into.
        static constexpr std::uint32_t kOutputChannels = 16;

        /// @brief Counts the state banks, one per side of a set crossfade.
        static constexpr std::uint32_t kBankCount = 2;

        /// @brief Allocates the ring, the scratch and both state banks; control thread.
        /// @param max_delay_frames Sized for the longest t_c the scene bounds allow: 5 * 12.0 /
        /// 343.0 s, which is 8397 frames at 48 kHz.
        /// @param max_block_frames The longest block write() and render() will ever see.
        void prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames);

        /// @brief Silences the ring and both state banks.
        void reset() noexcept;

        /// @brief Writes one block into the ring. Call once per block, before render().
        /// @pre frames <= max_block_frames.
        void write(const float* input, std::uint32_t frames) noexcept;

        /// @brief Adds this set's contribution into kOutputChannels accumulation buffers.
        /// @param bank 0 or 1: which biquad state bank to run, so a crossfade renders both.
        /// @param outputs kOutputChannels pointers to buffers of at least frames samples.
        /// @pre frames matches the block last given to write(); a tap's delay_frames lies in
        /// [1, max_delay_frames], the one frame being the Lagrange kernel's forward neighbour.
        void render(const EarlyTapSet& set, std::uint32_t bank,
                    float* const* outputs, std::uint32_t frames) noexcept;

        /// @brief Zeroes one state bank, for the set arriving into a crossfade. Audio thread.
        void clear_bank(std::uint32_t bank) noexcept;

        /// @brief Copies one tap's filter state from one bank into the same tap of another, so
        /// a tap arriving into a crossfade starts from its retiring state. The same bank as both
        /// ends leaves the tap as it is; a tap at or past kMaxEarlyTaps is ignored. Audio thread.
        void carry_tap(std::uint32_t from, std::uint32_t to, std::uint32_t tap) noexcept;

    private:
        /// @brief Counts the doubles one tap's two biquad histories take in a bank.
        static constexpr std::size_t kStatePerTap = 8;

        /// @brief Counts the frames the Lagrange kernel reads beyond the block: two behind the
        /// integer delay and one ahead of it.
        static constexpr std::uint32_t kKernelMargin = 4;

        /// @brief Returns the first double of one bank's state; a bank past kBankCount reads
        /// the last.
        double* bank_state(std::uint32_t bank) noexcept;

        /// @brief Reads one tap's four-point Lagrange interpolation, scaled by its gain, into
        /// the scratch buffer.
        void read_tap(const EarlyTap& tap, std::uint32_t frames) noexcept;

        /// @brief Runs the scratch buffer through both of one tap's sections, in place, from
        /// one bank's state.
        void filter_tap(const EarlyTap& tap, double* state, std::uint32_t frames) noexcept;

        /// @brief Multiply-adds the scratch buffer into every channel the tap encodes to.
        void encode_tap(const EarlyTap& tap, float* const* outputs,
                        std::uint32_t frames) noexcept;

        /// @brief Stores the input history followed by a mirror of its head, so every read
        /// window is contiguous.
        std::vector<float> ring_;

        /// @brief Stores one tap's block between its read, its filters and its encode.
        std::vector<float> scratch_;

        /// @brief Stores kBankCount banks of kStatePerTap doubles per tap, bank-major.
        std::vector<double> state_;

        /// @brief Counts the frames of history the ring keeps before the mirror region.
        std::uint32_t ring_length_ = 0;

        /// @brief Counts the frames the mirror region duplicates from the ring's start.
        std::uint32_t mirror_length_ = 0;

        /// @brief Indexes the ring frame the last written block started at.
        std::uint32_t block_start_ = 0;

        /// @brief Indexes the ring frame the next write lands on.
        std::uint32_t write_head_ = 0;

        /// @brief Bounds the integer delay a tap may ask for.
        std::uint32_t max_delay_frames_ = 0;

        /// @brief Bounds the block length prepare() sized the scratch for.
        std::uint32_t max_block_frames_ = 0;
    };
}
