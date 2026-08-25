// Fft.hpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Power-of-two, in-place-capable real FFT. Precomputes twiddle factors and bit-reversal in the constructor.
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
        void forward(const float* time_domain, float* real_out, float* imag_out) const;

        /// @brief Computes the inverse DFT back to a real time-domain signal.
        /// @param real_in Input real part, size() bins.
        /// @param imag_in Input imaginary part, size() bins.
        /// @param time_domain_out Output, size() samples.
        void inverse(const float* real_in, const float* imag_in, float* time_domain_out) const;

        /// @brief The transform size this plan was constructed for.
        /// @return The FFT size.
        std::uint32_t size() const;

    private:
        std::uint32_t              size_;
        std::vector<std::uint32_t> bit_reversal_;
        std::vector<float>         twiddle_real_;
        std::vector<float>         twiddle_imag_;
    };
}
