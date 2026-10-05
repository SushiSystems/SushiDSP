/****************************************************************/
/* FirFilter.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file FirFilter.hpp
 * @brief The block convolution one head-length impulse response runs at.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "SushiDSP/room/HrtfSet.hpp"

namespace SushiDSP::room
{
    /// @brief Convolves a block with kTaps coefficients over a history it keeps; the
    /// coefficients are copied in and the history survives a coefficient change. Audio thread.
    class FirFilter
    {
    public:
        /// @brief Counts the taps of one direct-path HRIR and of one diffuse-field filter.
        static constexpr std::uint32_t kTaps = HrtfSet::kTapCount;

        /// @brief Sizes the history for the longest block.
        void prepare(std::uint32_t max_block_frames);

        /// @brief Copies kTaps coefficients in.
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
        std::array<float, kTaps> taps_{};

        /// @brief Stores kTaps - 1 frames of history followed by room for one block.
        std::vector<float> history_;
    };
}
