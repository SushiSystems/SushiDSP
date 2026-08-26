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
/* permissions and limitations under the License.                        */
/**************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Interpolates by 2x using a windowed-sinc halfband FIR lowpass. Allocation-free
    /// after construction; carries streaming state across process() calls.
    ///
    /// The coefficients come from a Hamming-windowed sinc rather than an equiripple design.
    /// They are applied in polyphase form: the two output phases of a zero-stuffed input only
    /// ever touch the even- and odd-indexed taps respectively, so each phase is a dot product
    /// over the base-rate history alone, held in a ring buffer instead of a shifted delay
    /// line. The result is bit-identical to the direct-form FIR over the zero-stuffed signal
    /// (the skipped products are exact zeros, and the surviving terms are summed in the same
    /// order); tests pin that equivalence.
    class HalfbandUpsampler2x
    {
    public:
        /// @param taps Odd FIR length (default 15); larger is a sharper anti-imaging filter.
        explicit HalfbandUpsampler2x(std::uint32_t taps = 15);

        /// @brief Zero-stuffs input to 2x rate and lowpass-filters it.
        /// @param input frames input samples.
        /// @param frames Number of input frames.
        /// @param output 2*frames output samples; must not alias input.
        void process(const float* input, std::uint32_t frames, float* output);

        /// @brief Zeroes the history so the next process() starts from silence.
        void reset();

        /// @brief The FIR length actually in use, after the requested length was forced odd.
        /// @return Number of taps.
        std::uint32_t taps() const { return taps_; }

    private:
        std::vector<float> even_coefficients_;
        std::vector<float> odd_coefficients_;
        std::vector<float> history_;
        std::uint32_t      history_mask_;
        std::uint32_t      write_index_;
        std::uint32_t      taps_;
    };

    /// @brief Decimates by 2x using a windowed-sinc halfband FIR lowpass. Allocation-free
    /// after construction; carries streaming state across process() calls.
    ///
    /// Direct-form over a ring buffer: every tap is applied (the even-offset taps of a
    /// float-designed windowed sinc are near zero, not exactly zero, so dropping them would
    /// change the output) and only the retained output samples are computed.
    class HalfbandDownsampler2x
    {
    public:
        /// @param taps Odd FIR length (default 15).
        explicit HalfbandDownsampler2x(std::uint32_t taps = 15);

        /// @brief Lowpass-filters input then drops every other sample.
        /// @param input frames_in input samples.
        /// @param frames_in Number of input frames; must be even.
        /// @param output frames_in/2 output samples; must not alias input.
        void process(const float* input, std::uint32_t frames_in, float* output);

        /// @brief Zeroes the history so the next process() starts from silence.
        void reset();

        /// @brief The FIR length actually in use, after the requested length was forced odd.
        /// @return Number of taps.
        std::uint32_t taps() const { return taps_; }

    private:
        std::vector<float> coefficients_;
        std::vector<float> history_;
        std::uint32_t      history_mask_;
        std::uint32_t      write_index_;
        std::uint32_t      taps_;
    };

    /// @brief Cascades three halfband stages each way for 8x oversampling. Allocation-free
    /// after construction.
    class Oversampler8x
    {
    public:
        /// @param block_size The base-rate block size this instance will be called with.
        explicit Oversampler8x(std::uint32_t block_size);

        /// @brief Upsamples one base-rate block to 8x. Returns an internally-owned buffer.
        /// @param input block_size base-rate input samples.
        /// @param frames Must equal the block_size passed to the constructor.
        /// @return Pointer to frames*8 oversampled output samples, valid until the next call.
        const float* upsample(const float* input, std::uint32_t frames);

        /// @brief Downsamples an 8x-rate buffer back to the base rate.
        /// @param oversampled frames_oversampled samples at 8x rate.
        /// @param frames_oversampled Must equal 8 * the block_size passed to the constructor.
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
}
