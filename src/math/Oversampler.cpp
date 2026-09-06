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

#include <algorithm>
#include <vector>

#include "SushiDSP/math/Oversampler.hpp"

namespace SushiDSP
{
    namespace
    {
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

        /// @brief The non-zero taps on one side of the centre, nearest the centre first.
        ///
        /// A halfband of tap_count = 4M-1 has its centre at index 2M-1 and exact zeros at every
        /// even distance from it, so the surviving off-centre taps sit at distances 1, 3, ...,
        /// 2M-1. Symmetry makes the two sides equal, so one side is enough.
        std::vector<float> one_sided_pairs(const HalfbandDesign& design, float scale)
        {
            const std::uint32_t centre = (design.tap_count - 1) / 2;
            std::vector<float> pairs;
            for (std::uint32_t offset = 1; offset <= centre; offset += 2)
            {
                pairs.push_back(design.taps[centre - offset] * scale);
            }
            return pairs;
        }
    }

    HalfbandUpsampler2x::HalfbandUpsampler2x(const HalfbandDesign& design)
        : pair_coefficients_(one_sided_pairs(design, 2.0f))
        , centre_delay_((design.tap_count - 1) / 4)
        , history_(ring_capacity(design.tap_count / 2 + 2), 0.0f)
        , history_mask_(static_cast<std::uint32_t>(history_.size()) - 1)
        , write_index_(0)
        , taps_(design.tap_count)
    {
    }

    void HalfbandUpsampler2x::process(const float* input, std::uint32_t frames, float* output)
    {
        // tap_count = 4M-1, centre index c = 2M-1, and the zero-stuffed input has x_s[2n] = 2x[n]
        // with x_s odd samples zero. Output sample m only sees taps h[k] with k congruent to m
        // modulo 2.
        //
        // Output 2n+1 (odd) needs odd k, i.e. an even distance k-c, and every one of those is an
        // exact zero except the centre itself: y[2n+1] = 0.5 * x_s[2n+1-c] = x[n - (M-1)], a pure
        // delay of centre_delay_ = (tap_count-1)/4 = M-1 base-rate samples.
        //
        // Output 2n (even) needs even k, i.e. an odd distance d = 2k+1 from the centre, which is
        // exactly the non-zero pair set. The tap at k = c-d reads x[n - (centre_delay_ - k)] and
        // its mirror at k = c+d reads x[n - (centre_delay_ + k + 1)], so one coefficient serves
        // both and is multiplied once.
        const std::uint32_t pair_count = static_cast<std::uint32_t>(pair_coefficients_.size());
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            write_index_ = (write_index_ + 1) & history_mask_;
            history_[write_index_] = input[frame];

            // Accumulated in float. The 1e-6 direct-form equivalence the tests pin is
            // empirical - it holds because these designs' taps decay fast, so the pair
            // sum is dominated by a handful of terms - not a worst-case error bound.
            float accumulator = 0.0f;
            for (std::uint32_t k = 0; k < pair_count; ++k)
            {
                const float newer = history_[(write_index_ - (centre_delay_ - k)) & history_mask_];
                const float older = history_[(write_index_ - (centre_delay_ + k + 1)) & history_mask_];
                accumulator += pair_coefficients_[k] * (newer + older);
            }
            output[frame * 2] = accumulator;
            output[frame * 2 + 1] = history_[(write_index_ - centre_delay_) & history_mask_];
        }
    }

    void HalfbandUpsampler2x::reset()
    {
        std::fill(history_.begin(), history_.end(), 0.0f);
        write_index_ = 0;
    }

    HalfbandDownsampler2x::HalfbandDownsampler2x(const HalfbandDesign& design)
        : pair_coefficients_(one_sided_pairs(design, 1.0f))
        , centre_coefficient_(design.taps[(design.tap_count - 1) / 2])
        , centre_offset_((design.tap_count - 1) / 2)
        , history_(ring_capacity(design.tap_count + 1), 0.0f)
        , history_mask_(static_cast<std::uint32_t>(history_.size()) - 1)
        , write_index_(0)
        , taps_(design.tap_count)
    {
    }

    void HalfbandDownsampler2x::process(const float* input, std::uint32_t frames_in, float* output)
    {
        // Only the retained outputs y[m] = sum_k h[k] u[2m-k] are computed; the discarded input
        // samples still enter the history so it stays continuous. Within a retained output only
        // the non-zero taps are touched: the centre, at history offset centre_offset_, and the
        // symmetric pairs at odd distances 1, 3, 5, ... either side of it.
        const std::uint32_t pair_count = static_cast<std::uint32_t>(pair_coefficients_.size());
        std::uint32_t output_index = 0;
        for (std::uint32_t frame = 0; frame < frames_in; ++frame)
        {
            write_index_ = (write_index_ + 1) & history_mask_;
            history_[write_index_] = input[frame];
            if (frame % 2 == 0)
            {
                float accumulator = centre_coefficient_ * history_[(write_index_ - centre_offset_) & history_mask_];
                for (std::uint32_t k = 0; k < pair_count; ++k)
                {
                    const std::uint32_t offset = 2 * k + 1;
                    const float newer = history_[(write_index_ - (centre_offset_ - offset)) & history_mask_];
                    const float older = history_[(write_index_ - (centre_offset_ + offset)) & history_mask_];
                    accumulator += pair_coefficients_[k] * (newer + older);
                }
                output[output_index++] = accumulator;
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
        , stage1_up_(kHalfbandStage1)
        , stage2_up_(kHalfbandStage2)
        , stage3_up_(kHalfbandStage3)
        , stage1_down_(kHalfbandStage1)
        , stage2_down_(kHalfbandStage2)
        , stage3_down_(kHalfbandStage3)
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

    Oversampler4x::Oversampler4x(std::uint32_t block_size)
        : block_size_(block_size)
        , stage1_up_(kHalfbandStage1)
        , stage2_up_(kHalfbandStage2)
        , stage1_down_(kHalfbandStage1)
        , stage2_down_(kHalfbandStage2)
        , buffer_2x_(static_cast<std::size_t>(block_size) * 2, 0.0f)
        , buffer_4x_(static_cast<std::size_t>(block_size) * 4, 0.0f)
        , scratch_2x_(static_cast<std::size_t>(block_size) * 2, 0.0f)
    {
    }

    const float* Oversampler4x::upsample(const float* input, std::uint32_t frames)
    {
        stage1_up_.process(input, frames, buffer_2x_.data());
        stage2_up_.process(buffer_2x_.data(), frames * 2, buffer_4x_.data());
        return buffer_4x_.data();
    }

    void Oversampler4x::downsample(const float* oversampled, std::uint32_t frames_oversampled,
        float* output)
    {
        // Reverse of the upsample cascade: stage 2 is the one running at the 4x rate.
        stage2_down_.process(oversampled, frames_oversampled, scratch_2x_.data());
        stage1_down_.process(scratch_2x_.data(), frames_oversampled / 2, output);
    }

    std::uint32_t Oversampler4x::latency_samples() const
    {
        // A linear-phase FIR of T taps delays by (T-1)/2 samples at its own rate. Up-stages run
        // at 2x, 4x of base; down-stages at the same rates. Convert each to base-rate samples.
        const double up = (stage1_up_.taps() - 1) / 2.0 / 2.0
                        + (stage2_up_.taps() - 1) / 2.0 / 4.0;
        const double down = (stage2_down_.taps() - 1) / 2.0 / 4.0
                          + (stage1_down_.taps() - 1) / 2.0 / 2.0;
        return static_cast<std::uint32_t>(up + down);
    }

    void Oversampler4x::reset()
    {
        stage1_up_.reset();
        stage2_up_.reset();
        stage1_down_.reset();
        stage2_down_.reset();
        std::fill(buffer_2x_.begin(), buffer_2x_.end(), 0.0f);
        std::fill(buffer_4x_.begin(), buffer_4x_.end(), 0.0f);
        std::fill(scratch_2x_.begin(), scratch_2x_.end(), 0.0f);
    }
}
