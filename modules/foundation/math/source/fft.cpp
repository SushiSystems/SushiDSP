/**************************************************************************/
/* fft.cpp                                                                */
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
 * @file fft.cpp
 * @brief Implements the power-of-two real FFT.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include: it makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>
#include <utility>

#include <SushiDSP/foundation/math/fft.hpp>

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
        /// @tparam Inverse Whether the twiddles are conjugated and the result scaled by 1 / size.
        /// @param stage_twiddle_real The stages' twiddle cosines, stage span at offset span - 1.
        /// @param stage_twiddle_imag The stages' twiddle sines, laid out the same way.
        template <bool Inverse>
        void transform(float* const __restrict real, float* const __restrict imag,
                       std::uint32_t size, const std::uint32_t* const __restrict bit_reversal,
                       const float* const __restrict stage_twiddle_real,
                       const float* const __restrict stage_twiddle_imag)
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
                // The stage's twiddles are exp(-i*pi*k/span) for k in [0, span), contiguous.
                const float* const __restrict cosines = stage_twiddle_real + (span - 1);
                const float* const __restrict sines = stage_twiddle_imag + (span - 1);
                for (std::uint32_t group_start = 0; group_start < size; group_start += span * 2u)
                {
                    float* const __restrict even_real = real + group_start;
                    float* const __restrict even_imag = imag + group_start;
                    float* const __restrict odd_real_row = real + group_start + span;
                    float* const __restrict odd_imag_row = imag + group_start + span;
                    for (std::uint32_t k = 0; k < span; ++k)
                    {
                        const float cos_a = cosines[k];
                        // Forward uses exp(-i*theta); inverse conjugates it.
                        const float sin_a = Inverse ? -sines[k] : sines[k];

                        const float odd_real = odd_real_row[k] * cos_a - odd_imag_row[k] * sin_a;
                        const float odd_imag = odd_real_row[k] * sin_a + odd_imag_row[k] * cos_a;

                        odd_real_row[k] = even_real[k] - odd_real;
                        odd_imag_row[k] = even_imag[k] - odd_imag;
                        even_real[k] = even_real[k] + odd_real;
                        even_imag[k] = even_imag[k] + odd_imag;
                    }
                }
            }

            if (Inverse)
            {
                const float scale = 1.0f / static_cast<float>(size);
                for (std::uint32_t i = 0; i < size; ++i)
                {
                    real[i] *= scale;
                    imag[i] *= scale;
                }
            }
        }
    } // namespace

    Fft::Fft(std::uint32_t size)
        : size_(size), bit_reversal_(size), stage_twiddle_real_(size > 0 ? size - 1u : 0u),
          stage_twiddle_imag_(size > 0 ? size - 1u : 0u), real_scratch_(size), imag_scratch_(size)
    {
        assert(size > 0 && (size & (size - 1)) == 0 && "Fft size must be a power of two");
        const std::uint32_t bit_count = log2_of_power_of_two(size);
        for (std::uint32_t i = 0; i < size; ++i)
        {
            bit_reversal_[i] = reverse_bits(i, bit_count);
        }
        // The stages' twiddle tables sit end to end, span at offset span - 1 (modules/foundation/math/README.md).
        for (std::uint32_t span = 1; span < size; span <<= 1)
        {
            const std::uint32_t stride = size / (2u * span);
            for (std::uint32_t k = 0; k < span; ++k)
            {
                const double angle =
                    -2.0 * M_PI * static_cast<double>(k * stride) / static_cast<double>(size);
                stage_twiddle_real_[span - 1u + k] = static_cast<float>(std::cos(angle));
                stage_twiddle_imag_[span - 1u + k] = static_cast<float>(std::sin(angle));
            }
        }
    }

    void Fft::forward(const float* time_domain, float* real_out, float* imag_out) const
    {
        const std::uint32_t size = size_;
        const float* const __restrict source = time_domain;
        float* const __restrict real = real_out;
        float* const __restrict imag = imag_out;
        for (std::uint32_t i = 0; i < size; ++i)
        {
            real[i] = source[i];
            imag[i] = 0.0f;
        }
        transform<false>(real, imag, size, bit_reversal_.data(), stage_twiddle_real_.data(),
                         stage_twiddle_imag_.data());
    }

    void Fft::inverse(const float* real_in, const float* imag_in, float* time_domain_out) const
    {
        const std::uint32_t size = size_;
        const float* const __restrict real_source = real_in;
        const float* const __restrict imag_source = imag_in;
        float* const __restrict real = real_scratch_.data();
        float* const __restrict imag = imag_scratch_.data();
        for (std::uint32_t i = 0; i < size; ++i)
        {
            real[i] = real_source[i];
            imag[i] = imag_source[i];
        }
        transform<true>(real, imag, size, bit_reversal_.data(), stage_twiddle_real_.data(),
                        stage_twiddle_imag_.data());
        float* const __restrict destination = time_domain_out;
        for (std::uint32_t i = 0; i < size; ++i)
        {
            destination[i] = real[i];
        }
    }

    std::uint32_t Fft::size() const
    {
        return size_;
    }
} // namespace SushiDSP
