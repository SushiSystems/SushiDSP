// Fft.cpp
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and any earlier include could pull <cmath> in first and latch the definition out.
#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>
#include <utility>

#include "SushiDSP/math/Fft.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    namespace
    {
        std::uint32_t reverse_bits(std::uint32_t value, std::uint32_t bit_count)
        {
            std::uint32_t result = 0;
            for (std::uint32_t bit = 0; bit < bit_count; ++bit)
            {
                result = (result << 1) | (value & 1u);
                value >>= 1;
            }
            return result;
        }

        std::uint32_t log2_of_power_of_two(std::uint32_t value)
        {
            std::uint32_t log = 0;
            while ((1u << log) < value)
            {
                ++log;
            }
            return log;
        }

        /// @brief In-place radix-2 decimation-in-time Cooley-Tukey butterfly pass.
        ///
        /// The split-radix structure this class is eventually intended to carry is a drop-in
        /// replacement for this routine; correctness of the plan interface comes first.
        void transform(float*                            real,
                       float*                            imag,
                       std::uint32_t                     size,
                       const std::vector<std::uint32_t>& bit_reversal,
                       const std::vector<float>&         twiddle_real,
                       const std::vector<float>&         twiddle_imag,
                       bool                              inverse)
        {
            for (std::uint32_t i = 0; i < size; ++i)
            {
                const std::uint32_t j = bit_reversal[i];
                if (j > i)
                {
                    std::swap(real[i], real[j]);
                    std::swap(imag[i], imag[j]);
                }
            }

            for (std::uint32_t span = 1; span < size; span <<= 1)
            {
                // Twiddle table holds exp(-i*2*pi*k/size) for k in [0, size/2).
                // The butterfly at this stage needs exp(-i*pi*k/span), i.e. a stride of size/(2*span).
                const std::uint32_t stride = size / (2u * span);
                for (std::uint32_t group_start = 0; group_start < size; group_start += span * 2u)
                {
                    for (std::uint32_t k = 0; k < span; ++k)
                    {
                        const std::uint32_t t     = k * stride;
                        const float         cos_a = twiddle_real[t];
                        // Forward uses exp(-i*theta); inverse conjugates it.
                        const float sin_a = inverse ? -twiddle_imag[t] : twiddle_imag[t];

                        const std::uint32_t even = group_start + k;
                        const std::uint32_t odd  = even + span;

                        const float odd_real = real[odd] * cos_a - imag[odd] * sin_a;
                        const float odd_imag = real[odd] * sin_a + imag[odd] * cos_a;

                        real[odd]  = real[even] - odd_real;
                        imag[odd]  = imag[even] - odd_imag;
                        real[even] = real[even] + odd_real;
                        imag[even] = imag[even] + odd_imag;
                    }
                }
            }

            if (inverse)
            {
                const float scale = 1.0f / static_cast<float>(size);
                for (std::uint32_t i = 0; i < size; ++i)
                {
                    real[i] *= scale;
                    imag[i] *= scale;
                }
            }
        }
    }

    Fft::Fft(std::uint32_t size)
        : size_(size)
        , bit_reversal_(size)
        , twiddle_real_(size / 2u)
        , twiddle_imag_(size / 2u)
        , real_scratch_(size)
        , imag_scratch_(size)
    {
        assert(size > 0 && (size & (size - 1)) == 0 && "Fft size must be a power of two");
        const std::uint32_t bit_count = log2_of_power_of_two(size);
        for (std::uint32_t i = 0; i < size; ++i)
        {
            bit_reversal_[i] = reverse_bits(i, bit_count);
        }
        for (std::uint32_t k = 0; k < size / 2u; ++k)
        {
            const double angle = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(size);
            twiddle_real_[k]   = static_cast<float>(std::cos(angle));
            twiddle_imag_[k]   = static_cast<float>(std::sin(angle));
        }
    }

    void Fft::forward(const float* time_domain, float* real_out, float* imag_out) const
    {
        for (std::uint32_t i = 0; i < size_; ++i)
        {
            real_out[i] = time_domain[i];
            imag_out[i] = 0.0f;
        }
        transform(real_out, imag_out, size_, bit_reversal_, twiddle_real_, twiddle_imag_, false);
    }

    void Fft::inverse(const float* real_in, const float* imag_in, float* time_domain_out) const
    {
        for (std::uint32_t i = 0; i < size_; ++i)
        {
            real_scratch_[i] = real_in[i];
            imag_scratch_[i] = imag_in[i];
        }
        transform(real_scratch_.data(), imag_scratch_.data(), size_, bit_reversal_, twiddle_real_, twiddle_imag_, true);
        for (std::uint32_t i = 0; i < size_; ++i)
        {
            time_domain_out[i] = real_scratch_[i];
        }
    }

    std::uint32_t Fft::size() const
    {
        return size_;
    }
}
