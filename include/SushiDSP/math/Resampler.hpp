/**************************************************************************/
/* Resampler.hpp                                                          */
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

/**
 * @file Resampler.hpp
 * @brief Arbitrary-ratio polyphase resampler over the committed prototype filter.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Arbitrary-ratio polyphase resampler over one committed prototype filter.
    ///
    /// A single fractional phase accumulator walks the kernel of ResamplerCoefficients.hpp; a
    /// ratio change moves the accumulator's step and the kernel's time scale, and never rebuilds
    /// a filter bank. Every buffer is sized in prepare(); process() and set_rates() are
    /// allocation-, lock-, syscall- and throw-free.
    class Resampler
    {
    public:
        /// @brief Largest target/source ratio set_rates() will honour.
        static constexpr double kMaximumRatio = 16.0;

        /// @brief Sizes every buffer for the worst case this instance will be asked for.
        /// @param source_rate Initial input rate in Hz.
        /// @param target_rate Initial output rate in Hz.
        /// @param max_input_frames Largest input_frames a single process() call will carry.
        /// @param minimum_ratio Smallest target/source this instance will ever be set to; the
        /// kernel's span grows as 1/ratio when decimating, so this sizes the history.
        void prepare(double source_rate, double target_rate, std::uint32_t max_input_frames,
                     double minimum_ratio);

        /// @brief Changes the conversion rates. Real-time safe; allocates nothing.
        /// @param source_rate Input rate in Hz.
        /// @param target_rate Output rate in Hz.
        /// @pre prepare() has been called; the ratio is clamped to
        /// [minimum_ratio, kMaximumRatio].
        void set_rates(double source_rate, double target_rate);

        /// @brief Converts one push of input, producing however many outputs are now available.
        /// @param input input_frames source-rate samples.
        /// @param input_frames Number of input samples; must not exceed max_input_frames.
        /// @param output Destination, at least output_capacity samples; must not alias input.
        /// @param output_capacity Room available; use maximum_output_frames() to size it.
        /// @return Number of output samples written.
        std::uint32_t process(const float* input, std::uint32_t input_frames,
                              float* output, std::uint32_t output_capacity);

        /// @brief Upper bound on what process() can return for that many input frames.
        /// @param input_frames The push size.
        /// @return Output samples to make room for, at the current rates.
        std::uint32_t maximum_output_frames(std::uint32_t input_frames) const;

        /// @brief Algorithmic delay: an output can only be formed once the input half a kernel
        /// span past it has arrived. Equals kResamplerHalfWidthZeroCrossings (34, Task 1.1) /
        /// min(source_rate, target_rate) seconds at every ratio.
        /// @return Delay in seconds.
        double latency_seconds() const;

        /// @brief The same delay counted in source-rate samples, rounded to nearest.
        /// @return Delay in input samples.
        std::uint32_t latency_samples() const;

        /// @brief Zeroes the history and the phase, so the next process() starts from silence.
        void reset();

    private:
        /// @brief Reads the prototype filter between its stored entries.
        /// @param distance Distance from the peak in zero crossings, below the kernel's half width.
        /// @return The interpolated kernel value.
        double kernel_at(double distance) const;

        /// @brief Filters the history onto one output instant.
        /// @param origin Ring slot holding input sample 0 of the call in progress.
        /// @param t Output time in that call's input-sample coordinates.
        /// @return The output sample.
        double interpolate(std::uint32_t origin, double t) const;

        /// @brief Wraps a call-relative input index onto the history ring.
        /// @param origin Ring slot holding input sample 0 of the call in progress.
        /// @param offset Index relative to that sample; negative reaches into earlier calls.
        /// @return The ring slot.
        std::uint32_t ring_index(std::uint32_t origin, std::int32_t offset) const;

        std::vector<float> history_;
        std::uint32_t history_mask_ = 0u;
        std::uint32_t write_index_ = 0u;         // free-running; the ring length divides 2^32
        double position_ = 0.0;                  // next output, in the current call's coordinates
        double step_ = 1.0;                      // input samples advanced per output sample
        double scale_ = 1.0;                     // kernel time scale, min(1, ratio_)
        double half_span_ = 0.0;                 // kernel half width in input samples
        std::uint32_t taps_per_side_ = 0u;
        double ratio_ = 1.0;
        double minimum_ratio_ = 1.0;
        double source_rate_ = 0.0;
        double target_rate_ = 0.0;               // source_rate_ * ratio_, so it follows the clamp
        std::uint32_t max_input_frames_ = 0u;    // the push size prepare() sized the history for
    };
}
