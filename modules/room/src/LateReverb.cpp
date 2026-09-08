/**************************************************************************/
/* LateReverb.cpp                                                         */
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
 * @file LateReverb.cpp
 * @brief The delay lines, the absorption banks, the Walsh-Hadamard feedback and the two ears.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/LateReverb.hpp"

#include <algorithm>

#include "SushiDSP/core/DenormalGuard.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Counts the lines.
        constexpr std::size_t kLines = LateReverb::kLineCount;

        /// @brief Scales the 16-point Walsh-Hadamard butterfly sum to an orthonormal matrix.
        constexpr double kHadamardScale = 0.25;

        /// @brief Holds the left ear's output vector A: eight +1 and eight -1. Spec §6.4.
        constexpr std::array<double, kLines> kOutputA = {
            1.0, 1.0, -1.0, 1.0, -1.0, -1.0, 1.0, -1.0,
            1.0, -1.0, -1.0, 1.0, -1.0, 1.0, 1.0, -1.0
        };

        /// @brief Holds the output vector B, orthogonal to A and to the all-ones input vector.
        constexpr std::array<double, kLines> kOutputB = {
            1.0, -1.0, 1.0, 1.0, -1.0, 1.0, -1.0, -1.0,
            -1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0, -1.0
        };

        /// @brief Loads five design coefficients into one Biquad.
        void load_biquad(Biquad& biquad, const std::array<double, 5>& section) noexcept
        {
            biquad.set_coefficients(section[0], section[1], section[2], section[3], section[4]);
        }

        /// @brief Returns a read index length frames behind write in a ring of capacity frames.
        /// @pre length < capacity and write < capacity.
        inline std::uint32_t behind(std::uint32_t write, std::uint32_t length,
                                    std::uint32_t capacity) noexcept
        {
            const std::uint32_t index = write + capacity - length;
            return index >= capacity ? index - capacity : index;
        }
    }

    void LateReverb::SectionBank::run(const double* in, double* out) noexcept
    {
        const double* const __restrict x = in;
        double* const __restrict y = out;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            const double output = b0[line] * x[line] + b1[line] * x1[line] + b2[line] * x2[line]
                - a1[line] * y1[line] - a2[line] * y2[line];
            x2[line] = x1[line];
            x1[line] = x[line];
            y2[line] = y1[line];
            y1[line] = output;
            y[line] = output;
        }
    }

    void LateReverb::SectionBank::load(
        const std::array<std::array<double, 5>, kLineCount>& sections) noexcept
    {
        for (std::size_t line = 0; line < kLines; ++line)
        {
            b0[line] = sections[line][0];
            b1[line] = sections[line][1];
            b2[line] = sections[line][2];
            a1[line] = sections[line][3];
            a2[line] = sections[line][4];
        }
    }

    void LateReverb::SectionBank::reset() noexcept
    {
        x1.fill(0.0);
        x2.fill(0.0);
        y1.fill(0.0);
        y2.fill(0.0);
    }

    void LateReverb::hadamard_transform(std::array<double, kLineCount>& values) noexcept
    {
        double* const __restrict v = values.data();
        for (std::size_t half = 1; half < kLines; half <<= 1)
        {
            for (std::size_t block = 0; block < kLines; block += 2 * half)
            {
                for (std::size_t index = block; index < block + half; ++index)
                {
                    const double upper = v[index];
                    const double lower = v[index + half];
                    v[index] = upper + lower;
                    v[index + half] = upper - lower;
                }
            }
        }
        for (std::size_t index = 0; index < kLines; ++index)
        {
            v[index] *= kHadamardScale;
        }
    }

    void LateReverb::prepare(std::uint32_t max_line_frames)
    {
        capacity_ = std::max<std::uint32_t>(max_line_frames, 1u) + 1u;
        lines_.assign(static_cast<std::size_t>(kLines) * capacity_, 0.0f);
        input_delay_.assign(capacity_, 0.0f);
        reset();
    }

    void LateReverb::reset() noexcept
    {
        std::fill(lines_.begin(), lines_.end(), 0.0f);
        std::fill(input_delay_.begin(), input_delay_.end(), 0.0f);
        write_ = 0;
        fade_remaining_ = 0;
        fade_weight_ = 0.0;
        lengths_known_ = false;
        section_a_.reset();
        section_b_.reset();
        tone_left_.reset();
        tone_right_.reset();
        coherence_a_.reset();
        coherence_b_.reset();
    }

    void LateReverb::adopt_lengths(const FdnDesign& design) noexcept
    {
        std::array<std::uint32_t, kLineCount> wanted{};
        bool changed = false;
        for (std::size_t line = 0; line < kLines; ++line)
        {
            wanted[line] = std::clamp<std::uint32_t>(design.line_frames[line], 1u, capacity_ - 1u);
            changed |= wanted[line] != length_[line];
        }
        if (!lengths_known_)
        {
            length_ = wanted;
            previous_length_ = wanted;
            lengths_known_ = true;
            return;
        }
        if (changed)
        {
            // A change during a crossfade restarts it from the lengths the fade was heading to.
            previous_length_ = length_;
            length_ = wanted;
            fade_remaining_ = kCrossfadeFrames;
            fade_weight_ = 0.0;
        }
    }

    void LateReverb::load_filters(const FdnDesign& design) noexcept
    {
        section_a_.load(design.line_section_a);
        section_b_.load(design.line_section_b);
        load_biquad(tone_left_, design.tone_correction);
        load_biquad(tone_right_, design.tone_correction);
        load_biquad(coherence_a_, design.coherence_a);
        load_biquad(coherence_b_, design.coherence_b);
    }

    template <bool kFading, bool kTwoEars>
    void LateReverb::render(const float* input, float* left, float* right,
                            std::uint32_t frames) noexcept
    {
        const float* const __restrict in = input;
        float* const __restrict out_left = left;
        float* const __restrict out_right = right;
        float* const __restrict lines = lines_.data();
        float* const __restrict input_delay = input_delay_.data();
        const std::uint32_t* const __restrict length = length_.data();
        const std::uint32_t* const __restrict previous = previous_length_.data();
        const std::uint32_t capacity = capacity_;
        const std::uint32_t delay_frames = input_delay_frames_;
        const double gain = input_gain_;
        const double fade_step = 1.0 / static_cast<double>(kCrossfadeFrames);
        std::uint32_t write = write_;
        double weight = fade_weight_;
        std::array<double, kLineCount> read{};
        std::array<double, kLineCount> filtered{};

        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            input_delay[write] = in[frame];
            const double injected = gain * input_delay[behind(write, delay_frames, capacity)];

            for (std::size_t line = 0; line < kLines; ++line)
            {
                const float* const row = lines + static_cast<std::size_t>(line) * capacity;
                double value = row[behind(write, length[line], capacity)];
                if constexpr (kFading)
                {
                    const double old = row[behind(write, previous[line], capacity)];
                    value = old + weight * (value - old);
                }
                read[line] = value;
            }

            section_a_.run(read.data(), filtered.data());
            section_b_.run(filtered.data(), read.data());

            double sum_a = 0.0;
            double sum_b = 0.0;
            for (std::size_t line = 0; line < kLines; ++line)
            {
                sum_a += kOutputA[line] * read[line];
                sum_b += kOutputB[line] * read[line];
            }

            hadamard_transform(read);
            for (std::size_t line = 0; line < kLines; ++line)
            {
                lines[static_cast<std::size_t>(line) * capacity + write]
                    = static_cast<float>(read[line] + injected);
            }

            write = write + 1u == capacity ? 0u : write + 1u;
            if constexpr (kFading)
            {
                weight += fade_step;
            }

            const float left_sample = tone_left_.process_sample(static_cast<float>(sum_a));
            if constexpr (kTwoEars)
            {
                const float mixed = coherence_a_.process_sample(static_cast<float>(sum_a))
                    + coherence_b_.process_sample(static_cast<float>(sum_b));
                out_right[frame] = tone_right_.process_sample(mixed);
            }
            out_left[frame] = left_sample;
        }

        write_ = write;
        fade_weight_ = weight;
    }

    void LateReverb::render_run(bool fading, bool two_ears, const float* input, float* left,
                                float* right, std::uint32_t frames) noexcept
    {
        if (fading)
        {
            if (two_ears)
            {
                render<true, true>(input, left, right, frames);
            }
            else
            {
                render<true, false>(input, left, right, frames);
            }
        }
        else if (two_ears)
        {
            render<false, true>(input, left, right, frames);
        }
        else
        {
            render<false, false>(input, left, right, frames);
        }
    }

    void LateReverb::process(const FdnDesign& design, const float* input,
                             float* left, float* right, std::uint32_t frames) noexcept
    {
        DenormalGuard guard;
        adopt_lengths(design);
        load_filters(design);
        input_gain_ = static_cast<double>(design.input_gain);
        const float delay = std::max(design.delay_to_crossover_frames, 0.0f) + 0.5f;
        input_delay_frames_ = std::min<std::uint32_t>(static_cast<std::uint32_t>(delay),
                                                      capacity_ - 1u);

        const bool two_ears = left != right;
        std::uint32_t offset = 0;
        if (fade_remaining_ > 0)
        {
            const std::uint32_t fading = std::min(frames, fade_remaining_);
            render_run(true, two_ears, input, left, right, fading);
            fade_remaining_ -= fading;
            offset = fading;
        }
        render_run(false, two_ears, input + offset, left + offset, right + offset,
                   frames - offset);
    }
}
