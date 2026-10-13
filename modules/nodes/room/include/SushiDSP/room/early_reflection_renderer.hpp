/**************************************************************************/
/* early_reflection_renderer.hpp                                          */
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
 * @file early_reflection_renderer.hpp
 * @brief The audio-thread renderer that reads every early tap out of one input ring.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/delay_ring.hpp"
#include "SushiDSP/room/early_tap_set.hpp"

namespace SushiDSP::room
{
    /// @brief Reads up to 128 fractional taps out of a ring it is handed, filters each through two
    /// biquads from one of two state banks, four taps side by side, and encodes it into sixteen
    /// channels. Audio thread:
    /// allocation-free after prepare(). Spec §5.1.
    class EarlyReflectionRenderer
    {
        public:
            /// @brief Counts the accumulation buffers render() adds into.
            static constexpr std::uint32_t K_OUTPUT_CHANNELS = 16;

            /// @brief Counts the state banks, one per side of a set crossfade.
            static constexpr std::uint32_t K_BANK_COUNT = 2;

            /// @brief Allocates the scratch and both state banks; control thread.
            /// @param max_delay_frames Bounds a tap's delay: 5 * 12.0 / 343.0 s for the longest
            /// t_c the scene bounds allow, which is 8397 frames at 48 kHz.
            /// @param max_block_frames The longest block render() will ever see.
            void prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames);

            /// @brief Silences both state banks.
            void reset() noexcept;

            /// @brief Adds this set's contribution into K_OUTPUT_CHANNELS accumulation buffers.
            /// @param ring The ring the taps are read from, sized for max_delay_frames or more.
            /// @param bank 0 or 1: which biquad state bank to run, so a crossfade renders both.
            /// @param outputs K_OUTPUT_CHANNELS pointers to buffers of at least frames samples.
            /// @pre frames matches the block last written to the ring.
            void render(const DelayRing& ring, const EarlyTapSet& set, std::uint32_t bank,
                        float* const* outputs, std::uint32_t frames) noexcept;

            /// @brief Zeroes one state bank, for the set arriving into a crossfade. Audio thread.
            void clear_bank(std::uint32_t bank) noexcept;

            /// @brief Copies one tap's filter state from one bank into the same tap of another, so
            /// a tap arriving into a crossfade starts from its retiring state. The same bank as both
            /// ends leaves the tap as it is; a tap at or past K_MAX_EARLY_TAPS is ignored. Audio thread.
            void carry_tap(std::uint32_t from, std::uint32_t to, std::uint32_t tap) noexcept;

        private:
            /// @brief Counts the doubles one tap's two biquad histories take in a bank.
            static constexpr std::size_t K_STATE_PER_TAP = 8;

            /// @brief Returns the first double of one bank's state; a bank past K_BANK_COUNT reads
            /// the last.
            double* bank_state(std::uint32_t bank) noexcept;

            /// @brief Counts the taps one pass of the filters advances side by side.
            static constexpr std::uint32_t K_LANES = 4;

            /// @brief Reads one tap out of the ring at its delay, scaled by its gain, into one lane
            /// of the scratch buffer.
            void read_tap(const DelayRing& ring, const EarlyTap& tap, std::uint32_t lane,
                          std::uint32_t frames) noexcept;

            /// @brief Runs the first lanes of the scratch buffer through both sections of as many
            /// consecutive taps into the filtered buffer, each lane from its own tap's state:
            /// four lanes to a vector where the build has AVX, one lane after another where not.
            /// @param taps The first tap of the group.
            /// @param lanes The taps in the group, at most K_LANES.
            /// @param state The first tap's state; the others follow it.
            void filter_group(const EarlyTap* taps, std::uint32_t lanes, double* state,
                              std::uint32_t frames) noexcept;

            /// @brief Multiply-adds one lane of the filtered buffer into every channel the tap
            /// encodes to.
            void encode_tap(const EarlyTap& tap, std::uint32_t lane, float* const* outputs,
                            std::uint32_t frames) noexcept;

            /// @brief Stores K_LANES blocks, one tap each, between the read and the filters;
            /// lane-major.
            std::vector<float> scratch_;

            /// @brief Stores K_LANES blocks, one tap each, between the filters and the encode;
            /// lane-major. The filters do not write where they read.
            std::vector<float> filtered_;

            /// @brief Stores K_BANK_COUNT banks of K_STATE_PER_TAP doubles per tap, bank-major.
            std::vector<double> state_;

            /// @brief Bounds the integer delay a tap may ask for.
            std::uint32_t max_delay_frames_ = 0;

            /// @brief Bounds the block length prepare() sized the scratch for.
            std::uint32_t max_block_frames_ = 0;
    };
} // namespace SushiDSP::room
