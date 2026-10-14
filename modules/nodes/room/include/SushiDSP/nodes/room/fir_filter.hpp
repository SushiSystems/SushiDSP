/**************************************************************************/
/* fir_filter.hpp                                                         */
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
 * @file fir_filter.hpp
 * @brief The block convolution one head-length impulse response runs at.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/hrtf_set.hpp>

namespace SushiDSP::room
{
    /// @brief Convolves a block with K_TAPS coefficients over a history it keeps; the
    /// coefficients are copied in and the history survives a coefficient change. Audio thread.
    class FirFilter
    {
        public:
            /// @brief Counts the taps of one direct-path HRIR and of one diffuse-field filter.
            static constexpr std::uint32_t K_TAPS = HrtfSet::K_TAP_COUNT;

            /// @brief Sizes the history for the longest block.
            void prepare(std::uint32_t max_block_frames);

            /// @brief Copies K_TAPS coefficients in.
            void load(const float* taps) noexcept;

            /// @brief Zeroes the history.
            void reset() noexcept;

            /// @brief Copies another filter's history, not its coefficients.
            void carry_from(const FirFilter& other) noexcept;

            /// @brief Writes the convolution of one block into output.
            /// @pre frames <= max_block_frames; input and output do not alias.
            void process(const float* input, float* output, std::uint32_t frames) noexcept;

        private:
            /// @brief Stores the coefficients, tap 0 first.
            std::array<float, K_TAPS> taps_{};

            /// @brief Stores K_TAPS - 1 frames of history followed by room for one block.
            std::vector<float> history_;
    };
} // namespace SushiDSP::room
