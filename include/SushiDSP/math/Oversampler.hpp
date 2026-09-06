/**************************************************************************/
/* Oversampler.hpp                                                        */
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

#include "SushiDSP/math/HalfbandCoefficients.hpp"

namespace SushiDSP
{
    /// @brief 2x interpolator on a committed Parks-McClellan halfband. Allocation-free
    /// after construction; carries streaming state across process() calls.
    ///
    /// Polyphase: one output phase holds only the centre tap (every other tap of that phase is
    /// an exact zero), so it is the input delayed by (tap_count-1)/4 base-rate samples; the
    /// other phase is a dot product of the non-zero taps against the base-rate history, summed
    /// as symmetric pairs so each coefficient is multiplied once.
    class HalfbandUpsampler2x
    {
    public:
        /// @param design Which committed halfband to run; every stage of Oversampler8x names its own.
        explicit HalfbandUpsampler2x(const HalfbandDesign& design = kHalfbandStage1);

        /// @brief Zero-stuffs input to 2x rate and lowpass-filters it.
        /// @param input frames input samples.
        /// @param frames Number of input frames.
        /// @param output 2*frames output samples; must not alias input.
        void process(const float* input, std::uint32_t frames, float* output);

        /// @brief Zeroes the history so the next process() starts from silence.
        void reset();

        /// @brief The full FIR length of the design in use.
        /// @return Number of taps.
        std::uint32_t taps() const { return taps_; }

    private:
        std::vector<float> pair_coefficients_;   // non-zero taps left of centre, nearest first, times the 2x stuffing gain
        std::uint32_t      centre_delay_;        // base-rate samples between the newest input and the centre tap
        std::vector<float> history_;
        std::uint32_t      history_mask_;
        std::uint32_t      write_index_;
        std::uint32_t      taps_;
    };

    /// @brief 2x decimator on a committed Parks-McClellan halfband. Allocation-free after
    /// construction; carries streaming state across process() calls.
    ///
    /// Only the retained output samples are computed, and for each only the non-zero taps
    /// are touched: the centre tap and the symmetric pairs at odd offsets from it.
    class HalfbandDownsampler2x
    {
    public:
        /// @param design Which committed halfband to run.
        explicit HalfbandDownsampler2x(const HalfbandDesign& design = kHalfbandStage1);

        /// @brief Lowpass-filters input then drops every other sample.
        /// @param input frames_in input samples.
        /// @param frames_in Number of input frames; must be even.
        /// @param output frames_in/2 output samples; must not alias input.
        void process(const float* input, std::uint32_t frames_in, float* output);

        /// @brief Zeroes the history so the next process() starts from silence.
        void reset();

        /// @brief The full FIR length of the design in use.
        /// @return Number of taps.
        std::uint32_t taps() const { return taps_; }

    private:
        std::vector<float> pair_coefficients_;   // taps at odd offsets 1, 3, 5, ... from the centre
        float              centre_coefficient_;
        std::uint32_t      centre_offset_;       // (tap_count - 1) / 2
        std::vector<float> history_;
        std::uint32_t      history_mask_;
        std::uint32_t      write_index_;
        std::uint32_t      taps_;
    };

    /// @brief Cascades three committed Parks-McClellan halfband stages (see
    /// HalfbandCoefficients.hpp) each way for 8x oversampling. Allocation-free after
    /// construction.
    class Oversampler8x
    {
    public:
        /// @param block_size The base-rate block size this instance will be called with.
        explicit Oversampler8x(std::uint32_t block_size);

        /// @brief Upsamples one base-rate block to 8x. Returns an internally-owned buffer.
        /// @param input block_size base-rate input samples.
        /// @param frames Must not exceed the block_size passed to the constructor.
        /// @return Pointer to frames*8 oversampled output samples, valid until the next call.
        const float* upsample(const float* input, std::uint32_t frames);

        /// @brief Downsamples an 8x-rate buffer back to the base rate.
        /// @param oversampled frames_oversampled samples at 8x rate.
        /// @param frames_oversampled Must not exceed 8 * the block_size passed to the constructor.
        /// @param output frames_oversampled/8 base-rate output samples.
        void downsample(const float* oversampled, std::uint32_t frames_oversampled, float* output);

        /// @brief Resets every stage's history.
        void reset();

        /// @brief The base-rate block size this instance was constructed for.
        std::uint32_t block_size() const { return block_size_; }

        /// @brief Round-trip group delay of the three up and three down stages.
        /// @return Delay in base-rate samples (rounded down).
        std::uint32_t latency_samples() const;

    private:
        std::uint32_t block_size_;
        HalfbandUpsampler2x stage1_up_, stage2_up_, stage3_up_;
        HalfbandDownsampler2x stage1_down_, stage2_down_, stage3_down_;
        std::vector<float> buffer_2x_;
        std::vector<float> buffer_4x_;
        std::vector<float> buffer_8x_;
        std::vector<float> scratch_4x_;
        std::vector<float> scratch_2x_;
    };

    /// @brief Cascades the first two committed Parks-McClellan halfband stages (see
    /// HalfbandCoefficients.hpp) each way for 4x oversampling. Allocation-free after
    /// construction.
    class Oversampler4x
    {
    public:
        /// @param block_size The base-rate block size this instance will be called with.
        explicit Oversampler4x(std::uint32_t block_size);

        /// @brief Upsamples one base-rate block to 4x. Returns an internally-owned buffer.
        /// @param input block_size base-rate input samples.
        /// @param frames Must not exceed the block_size passed to the constructor.
        /// @return Pointer to frames*4 oversampled output samples, valid until the next call.
        const float* upsample(const float* input, std::uint32_t frames);

        /// @brief Downsamples a 4x-rate buffer back to the base rate.
        /// @param oversampled frames_oversampled samples at 4x rate.
        /// @param frames_oversampled Must not exceed 4 * the block_size passed to the constructor.
        /// @param output frames_oversampled/4 base-rate output samples.
        void downsample(const float* oversampled, std::uint32_t frames_oversampled, float* output);

        /// @brief Resets every stage's history.
        void reset();

        /// @brief The base-rate block size this instance was constructed for.
        std::uint32_t block_size() const { return block_size_; }

        /// @brief Round-trip group delay of the two up and two down stages.
        /// @return Delay in base-rate samples (rounded down).
        std::uint32_t latency_samples() const;

    private:
        std::uint32_t block_size_;
        HalfbandUpsampler2x stage1_up_, stage2_up_;
        HalfbandDownsampler2x stage1_down_, stage2_down_;
        std::vector<float> buffer_2x_;
        std::vector<float> buffer_4x_;
        std::vector<float> scratch_2x_;
    };
}
