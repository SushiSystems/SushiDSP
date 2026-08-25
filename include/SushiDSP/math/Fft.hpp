/**************************************************************************/
/* Fft.hpp                                                                */
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
        std::uint32_t              size_;
        std::vector<std::uint32_t> bit_reversal_;
        std::vector<float>         twiddle_real_;
        std::vector<float>         twiddle_imag_;

        // Sized once in the constructor so inverse() can stay const and allocation-free.
        mutable std::vector<float> real_scratch_;
        mutable std::vector<float> imag_scratch_;
    };
}
