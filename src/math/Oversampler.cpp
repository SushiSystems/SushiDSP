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
/* permissions and limitations under the License.                        */
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

        /// @brief Every other coefficient starting at `first`, in ascending tap order.
        std::vector<float> take_every_other(const std::vector<float>& coefficients, std::size_t first)
        {
            std::vector<float> subset;
            for (std::size_t i = first; i < coefficients.size(); i += 2)
            {
                subset.push_back(coefficients[i]);
            }
            return subset;
        }

        /// @brief Smallest power of two that holds at least `minimum` samples, so the ring
        /// index can wrap with a mask.
        std::uint32_t ring_capacity(std::size_t minimum)
        {
            std::uint32_t capacity = 1;
            while (capacity < minimum)
            {
                capacity *= 2;
            }
            return capacity;
        }

        /// @brief Dot product of `coefficients` against the ring's history, newest sample
        /// first. Terms are accumulated in ascending tap order, which is what keeps this
        /// bit-identical to the shifted-delay-line direct form it replaced.
        float dot_with_history(const std::vector<float>& coefficients, const std::vector<float>& history,
            std::uint32_t newest_index, std::uint32_t mask)
        {
            float accumulator = 0.0f;
            const std::uint32_t count = static_cast<std::uint32_t>(coefficients.size());
            for (std::uint32_t i = 0; i < count; ++i)
            {
                accumulator += coefficients[i] * history[(newest_index - i) & mask];
            }
            return accumulator;
        }
    }

    HalfbandUpsampler2x::HalfbandUpsampler2x(std::uint32_t taps)
        : even_coefficients_(take_every_other(design_halfband(taps), 0))
        , odd_coefficients_(take_every_other(design_halfband(taps), 1))
        , history_(ring_capacity(even_coefficients_.size()), 0.0f)
        , history_mask_(static_cast<std::uint32_t>(history_.size()) - 1)
        , write_index_(0)
        , taps_(static_cast<std::uint32_t>(even_coefficients_.size() + odd_coefficients_.size()))
    {
    }

    void HalfbandUpsampler2x::process(const float* input, std::uint32_t frames, float* output)
    {
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            write_index_ = (write_index_ + 1) & history_mask_;
            // The 2x factor compensates the energy lost to zero-stuffing, so DC comes through
            // the unity-DC-gain filter at unity gain.
            history_[write_index_] = input[frame] * 2.0f;
            output[frame * 2] = dot_with_history(even_coefficients_, history_, write_index_, history_mask_);
            output[frame * 2 + 1] = dot_with_history(odd_coefficients_, history_, write_index_, history_mask_);
        }
    }

    void HalfbandUpsampler2x::reset()
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
        write_index_ = 0;
    }

    HalfbandDownsampler2x::HalfbandDownsampler2x(std::uint32_t taps)
        : coefficients_(design_halfband(taps))
        , history_(ring_capacity(coefficients_.size()), 0.0f)
        , history_mask_(static_cast<std::uint32_t>(history_.size()) - 1)
        , write_index_(0)
        , taps_(static_cast<std::uint32_t>(coefficients_.size()))
    {
    }

    void HalfbandDownsampler2x::process(const float* input, std::uint32_t frames_in, float* output)
    {
        std::uint32_t output_index = 0;
        for (std::uint32_t frame = 0; frame < frames_in; ++frame)
        {
            write_index_ = (write_index_ + 1) & history_mask_;
            history_[write_index_] = input[frame];
            // Only every other filtered sample is kept; the discarded ones still enter the
            // history so it stays continuous.
            if (frame % 2 == 0)
            {
                output[output_index++] = dot_with_history(coefficients_, history_, write_index_, history_mask_);
            }
        }
    }

    void HalfbandDownsampler2x::reset()
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
        write_index_ = 0;
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

    std::uint32_t Oversampler8x::latency_samples() const
    {
        // A linear-phase FIR of T taps delays by (T-1)/2 samples at its own rate. Up-stages run
        // at 2x, 4x, 8x of base; down-stages at the same rates. Convert each to base-rate samples.
        const double up = (stage1_up_.taps() - 1) / 2.0 / 2.0
                        + (stage2_up_.taps() - 1) / 2.0 / 4.0
                        + (stage3_up_.taps() - 1) / 2.0 / 8.0;
        const double down = (stage3_down_.taps() - 1) / 2.0 / 8.0
                          + (stage2_down_.taps() - 1) / 2.0 / 4.0
                          + (stage1_down_.taps() - 1) / 2.0 / 2.0;
        return static_cast<std::uint32_t>(up + down);
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
