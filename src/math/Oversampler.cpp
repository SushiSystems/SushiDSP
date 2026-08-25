/**************************************************************************/
/* Oversampler.cpp                                                        */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and it must remain the very first thing in the translation unit.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>

#include "SushiDSP/math/Oversampler.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    namespace
    {
        /// @brief Hamming-windowed sinc halfband lowpass, cutoff at half the current Nyquist.
        ///
        /// The taps sum to 1, so the filter's DC gain is unity; the ideal response is 0.5 at the
        /// cutoff and near zero at Nyquist. taps is forced odd so a single centre tap exists.
        std::vector<float> design_halfband(std::uint32_t taps)
        {
            if (taps < 3)
            {
                taps = 3;
            }
            if (taps % 2 == 0)
            {
                ++taps;
            }

            std::vector<float> coefficients(taps, 0.0f);
            const int center = static_cast<int>(taps - 1) / 2;
            for (int n = 0; n < static_cast<int>(taps); ++n)
            {
                const int k = n - center;
                float sinc_value;
                if (k == 0)
                {
                    sinc_value = 0.5f;
                }
                else
                {
                    sinc_value = std::sin(static_cast<float>(M_PI) * static_cast<float>(k) / 2.0f)
                        / (static_cast<float>(M_PI) * static_cast<float>(k));
                }
                const float window = 0.54f
                    - 0.46f * std::cos(2.0f * static_cast<float>(M_PI) * static_cast<float>(n)
                        / static_cast<float>(taps - 1));
                coefficients[n] = sinc_value * window;
            }
            return coefficients;
        }
    }

    HalfbandUpsampler2x::HalfbandUpsampler2x(std::uint32_t taps)
        : coefficients_(design_halfband(taps))
        , delay_line_(coefficients_.size(), 0.0f)
    {
    }

    void HalfbandUpsampler2x::process(const float* input, std::uint32_t frames, float* output)
    {
        const std::uint32_t taps = static_cast<std::uint32_t>(coefficients_.size());
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            for (std::uint32_t sub = 0; sub < 2; ++sub)
            {
                for (std::uint32_t i = taps - 1; i > 0; --i)
                {
                    delay_line_[i] = delay_line_[i - 1];
                }
                // The 2x factor compensates the energy lost to zero-stuffing, so DC comes
                // through the unity-DC-gain filter at unity gain.
                delay_line_[0] = (sub == 0) ? input[frame] * 2.0f : 0.0f;

                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients_[i] * delay_line_[i];
                }
                output[frame * 2 + sub] = accumulator;
            }
        }
    }

    void HalfbandUpsampler2x::reset()
    {
        std::fill(delay_line_.begin(), delay_line_.end(), 0.0f);
    }

    HalfbandDownsampler2x::HalfbandDownsampler2x(std::uint32_t taps)
        : coefficients_(design_halfband(taps))
        , delay_line_(coefficients_.size(), 0.0f)
    {
    }

    void HalfbandDownsampler2x::process(const float* input, std::uint32_t frames_in, float* output)
    {
        const std::uint32_t taps = static_cast<std::uint32_t>(coefficients_.size());
        std::uint32_t output_index = 0;
        for (std::uint32_t frame = 0; frame < frames_in; ++frame)
        {
            for (std::uint32_t i = taps - 1; i > 0; --i)
            {
                delay_line_[i] = delay_line_[i - 1];
            }
            delay_line_[0] = input[frame];

            // Only every other filtered sample is kept; the discarded ones still have to be
            // shifted in so the delay line stays continuous.
            if (frame % 2 == 0)
            {
                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients_[i] * delay_line_[i];
                }
                output[output_index++] = accumulator;
            }
        }
    }

    void HalfbandDownsampler2x::reset()
    {
        std::fill(delay_line_.begin(), delay_line_.end(), 0.0f);
    }

    Oversampler8x::Oversampler8x(std::uint32_t block_size)
        : block_size_(block_size)
        , buffer_2x_(static_cast<std::size_t>(block_size) * 2, 0.0f)
        , buffer_4x_(static_cast<std::size_t>(block_size) * 4, 0.0f)
        , buffer_8x_(static_cast<std::size_t>(block_size) * 8, 0.0f)
        , scratch_4x_(static_cast<std::size_t>(block_size) * 4, 0.0f)
        , scratch_2x_(static_cast<std::size_t>(block_size) * 2, 0.0f)
    {
    }

    const float* Oversampler8x::upsample(const float* input, std::uint32_t frames)
    {
        stage1_up_.process(input, frames, buffer_2x_.data());
        stage2_up_.process(buffer_2x_.data(), frames * 2, buffer_4x_.data());
        stage3_up_.process(buffer_4x_.data(), frames * 4, buffer_8x_.data());
        return buffer_8x_.data();
    }

    void Oversampler8x::downsample(const float* oversampled, std::uint32_t frames_oversampled,
        float* output)
    {
        // Reverse of the upsample cascade: stage 3 is the one running at the 8x rate.
        stage3_down_.process(oversampled, frames_oversampled, scratch_4x_.data());
        stage2_down_.process(scratch_4x_.data(), frames_oversampled / 2, scratch_2x_.data());
        stage1_down_.process(scratch_2x_.data(), frames_oversampled / 4, output);
    }

    void Oversampler8x::reset()
    {
        stage1_up_.reset();
        stage2_up_.reset();
        stage3_up_.reset();
        stage1_down_.reset();
        stage2_down_.reset();
        stage3_down_.reset();
        std::fill(buffer_2x_.begin(), buffer_2x_.end(), 0.0f);
        std::fill(buffer_4x_.begin(), buffer_4x_.end(), 0.0f);
        std::fill(buffer_8x_.begin(), buffer_8x_.end(), 0.0f);
        std::fill(scratch_4x_.begin(), scratch_4x_.end(), 0.0f);
        std::fill(scratch_2x_.begin(), scratch_2x_.end(), 0.0f);
    }
}
