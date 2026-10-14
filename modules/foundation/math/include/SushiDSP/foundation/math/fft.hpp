/**************************************************************************/
/* fft.hpp                                                                */
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
 * @file fft.hpp
 * @brief Declares a power-of-two real FFT.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Power-of-two real FFT. Precomputes twiddle factors and bit-reversal in the constructor.
    ///
    /// Transforms are out-of-place: the three buffers passed to forward() or inverse() must be
    /// distinct and must not alias each other.
    class Fft
    {
        public:
            /// @brief Constructs an FFT plan for the given power-of-two size.
            /// @param size Transform size; must be a power of two.
            explicit Fft(std::uint32_t size);

            /// @brief Computes the forward DFT of a real time-domain signal.
            /// @param time_domain Input, size() samples.
            /// @param real_out Output real part, size() bins.
            /// @param imag_out Output imaginary part, size() bins.
            /// @note The three buffers must not alias each other.
            void forward(const float* time_domain, float* real_out, float* imag_out) const;

            /// @brief Computes the inverse DFT back to a real time-domain signal.
            /// @param real_in Input real part, size() bins.
            /// @param imag_in Input imaginary part, size() bins.
            /// @param time_domain_out Output, size() samples.
            /// @note The three buffers must not alias each other.
            /// @note Uses internal scratch buffers, so concurrent calls on one plan are not supported.
            void inverse(const float* real_in, const float* imag_in, float* time_domain_out) const;

            /// @brief The transform size this plan was constructed for.
            /// @return The FFT size.
            std::uint32_t size() const;

        private:
            std::uint32_t size_;
            std::vector<std::uint32_t> bit_reversal_;

            // Every butterfly stage's twiddles laid end to end, stage span at offset span - 1.
            std::vector<float> stage_twiddle_real_;
            std::vector<float> stage_twiddle_imag_;

            // Sized once in the constructor so inverse() can stay const and allocation-free.
            mutable std::vector<float> real_scratch_;
            mutable std::vector<float> imag_scratch_;
    };
} // namespace SushiDSP
