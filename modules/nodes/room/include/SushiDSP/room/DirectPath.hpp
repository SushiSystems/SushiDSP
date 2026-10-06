/**************************************************************************/
/* DirectPath.hpp                                                         */
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
 * @file DirectPath.hpp
 * @brief The sound that reaches the ears straight from the cabinet, through its HRIR pair.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/DelayRing.hpp"
#include "SushiDSP/room/EarlyReflectionRenderer.hpp"
#include "SushiDSP/room/EarlyTapSet.hpp"
#include "SushiDSP/room/FadeWindow.hpp"
#include "SushiDSP/room/FirFilter.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"

namespace SushiDSP::room
{
    /// @brief Renders the ears' direct sound from one bank's coefficients: the path's ring
    /// read at the alignment delay plus the tap's, the far ear also at the ITD, the tap's
    /// fitted filter and gain, and the HRIR pair. Two banks, so a swap crossfades. Audio
    /// thread.
    class DirectPath
    {
    public:
        /// @brief Counts the state banks, one per side of a crossfade.
        static constexpr std::uint32_t kBanks = EarlyReflectionRenderer::kBankCount;

        /// @brief Sizes both banks' histories and records the alignment delay.
        void prepare(std::uint32_t alignment_frames, std::uint32_t max_block_frames);

        /// @brief Zeroes both banks' state and deactivates both.
        void reset() noexcept;

        /// @brief Loads one bank with a tap and a set's HRIR pair and ITD, and activates it.
        void load_bank(std::uint32_t bank, const EarlyTap& tap, const RoomRenderSet& set) noexcept;

        /// @brief Zeroes one bank's state and deactivates it.
        void clear_bank(std::uint32_t bank) noexcept;

        /// @brief Copies one bank's filter and history state into another.
        void carry_bank(std::uint32_t from, std::uint32_t to) noexcept;

        /// @brief Returns the tap one bank was loaded with.
        const EarlyTap& tap(std::uint32_t bank) const noexcept;

        /// @brief Reports whether one bank renders anything.
        bool active(std::uint32_t bank) const noexcept;

        /// @brief Writes one bank's direct sound to both ears, read out of the ring the
        /// block was written to; an inactive bank writes zeros.
        void render(const DelayRing& ring, std::uint32_t bank, float* left, float* right,
                    std::uint32_t frames) noexcept;

    private:
        /// @brief Holds one ear's chain in one bank: the fitted filter's state and the HRIR.
        struct Ear
        {
            /// @brief Stores the two sections' histories, in the early renderer's layout.
            std::array<double, 8> filter_state;

            /// @brief Convolves with the ear's HRIR.
            FirFilter hrir;

            /// @brief Stores the read delay in frames past the tap's, the ITD on the far ear.
            double extra_delay;
        };

        /// @brief Holds one bank: the tap, the two ears and whether it renders.
        struct Bank
        {
            /// @brief Stores the tap, its delay counted from the alignment delay.
            EarlyTap tap;

            /// @brief Stores the left and right chains.
            std::array<Ear, kEars> ears;

            /// @brief Records that the bank holds an ears set's direct path.
            bool active;
        };

        /// @brief Reads, filters and convolves one ear of one bank into output.
        void render_ear(const DelayRing& ring, Bank& bank, std::uint32_t ear, float* output,
                        std::uint32_t frames) noexcept;

        /// @brief Stores the two banks.
        std::array<Bank, kBanks> banks_{};

        /// @brief Stores one ear's block between the read and the convolution.
        std::vector<float> scratch_;

        /// @brief Stores the frames every read is delayed by, the decoder's partition.
        std::uint32_t alignment_frames_ = 0;
    };
}
