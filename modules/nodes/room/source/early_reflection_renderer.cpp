/**************************************************************************/
/* early_reflection_renderer.cpp                                          */
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
 * @file early_reflection_renderer.cpp
 * @brief The mirrored input ring, the Lagrange read, the two biquads and the encode per tap.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/early_reflection_renderer.hpp"

#include <algorithm>
#include <cmath>

#include "SushiDSP/core/denormal_guard.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the four Lagrange weights of one tap, already scaled by its gain.
        struct LagrangeKernel
        {
                /// @brief Stores the weight of the sample two frames behind the integer delay.
                float c0;

                /// @brief Stores the weight of the sample one frame behind the integer delay.
                float c1;

                /// @brief Stores the weight of the sample at the integer delay.
                float c2;

                /// @brief Stores the weight of the sample one frame ahead of the integer delay.
                float c3;
        };

        /// @brief Returns the third-order Lagrange weights for a read at fraction frac behind
        /// the integer delay, scaled by gain; frac of zero yields exactly { 0, 0, gain, 0 }.
        LagrangeKernel lagrange_kernel(double frac, double gain) noexcept
        {
            const double s = 2.0 - frac;
            const double s1 = s - 1.0;
            const double s2 = s - 2.0;
            const double s3 = s - 3.0;
            return LagrangeKernel{static_cast<float>(-s1 * s2 * s3 * gain / 6.0),
                                  static_cast<float>(s * s2 * s3 * gain / 2.0),
                                  static_cast<float>(-s * s1 * s3 * gain / 2.0),
                                  static_cast<float>(s * s1 * s2 * gain / 6.0)};
        }
    } // namespace

    void EarlyReflectionRenderer::prepare(std::uint32_t max_delay_frames,
                                          std::uint32_t max_block_frames)
    {
        max_delay_frames_ = max_delay_frames;
        max_block_frames_ = max_block_frames;
        mirror_length_ = max_block_frames + K_KERNEL_MARGIN;
        ring_length_ = max_delay_frames + mirror_length_;
        ring_.assign(static_cast<std::size_t>(ring_length_) + mirror_length_, 0.0f);
        scratch_.assign(max_block_frames, 0.0f);
        state_.assign(static_cast<std::size_t>(K_BANK_COUNT) * EarlyTapSet::K_MAX_EARLY_TAPS *
                          K_STATE_PER_TAP,
                      0.0);
        block_start_ = 0;
        write_head_ = 0;
    }

    void EarlyReflectionRenderer::reset() noexcept
    {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        std::fill(state_.begin(), state_.end(), 0.0);
        block_start_ = 0;
        write_head_ = 0;
    }

    void EarlyReflectionRenderer::write(const float* input, std::uint32_t frames) noexcept
    {
        frames = std::min(frames, max_block_frames_);
        block_start_ = write_head_;
        float* const ring = ring_.data();
        std::uint32_t written = 0;
        while (written < frames)
        {
            const std::uint32_t index = write_head_;
            const std::uint32_t run = std::min(frames - written, ring_length_ - index);
            std::copy(input + written, input + written + run, ring + index);
            // Frames landing under the mirror region are written again one ring length up.
            const std::uint32_t mirrored =
                index < mirror_length_ ? std::min(run, mirror_length_ - index) : 0u;
            std::copy(input + written, input + written + mirrored, ring + ring_length_ + index);
            written += run;
            write_head_ = index + run == ring_length_ ? 0u : index + run;
        }
    }

    void EarlyReflectionRenderer::render(const EarlyTapSet& set, std::uint32_t bank,
                                         float* const* outputs, std::uint32_t frames) noexcept
    {
        const DenormalGuard guard;
        frames = std::min(frames, max_block_frames_);
        const std::uint32_t count = std::min<std::uint32_t>(
            set.count, static_cast<std::uint32_t>(EarlyTapSet::K_MAX_EARLY_TAPS));
        double* const state = bank_state(bank);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const EarlyTap& tap = set.taps[index];
            read_tap(tap, frames);
            filter_tap(tap, state + static_cast<std::size_t>(index) * K_STATE_PER_TAP, frames);
            encode_tap(tap, outputs, frames);
        }
    }

    void EarlyReflectionRenderer::clear_bank(std::uint32_t bank) noexcept
    {
        double* const state = bank_state(bank);
        std::fill(state, state + EarlyTapSet::K_MAX_EARLY_TAPS * K_STATE_PER_TAP, 0.0);
    }

    void EarlyReflectionRenderer::carry_tap(std::uint32_t from, std::uint32_t to,
                                            std::uint32_t tap) noexcept
    {
        if (tap >= EarlyTapSet::K_MAX_EARLY_TAPS)
        {
            return;
        }
        const std::size_t offset = static_cast<std::size_t>(tap) * K_STATE_PER_TAP;
        const double* const source = bank_state(from) + offset;
        double* const destination = bank_state(to) + offset;
        // The same bank as both ends is a copy onto itself, which changes nothing.
        std::copy(source, source + K_STATE_PER_TAP, destination);
    }

    double* EarlyReflectionRenderer::bank_state(std::uint32_t bank) noexcept
    {
        return state_.data() + static_cast<std::size_t>(std::min(bank, K_BANK_COUNT - 1u)) *
                                   EarlyTapSet::K_MAX_EARLY_TAPS * K_STATE_PER_TAP;
    }

    void EarlyReflectionRenderer::read_tap(const EarlyTap& tap, std::uint32_t frames) noexcept
    {
        const double delay = std::min(std::max(static_cast<double>(tap.delay_frames), 1.0),
                                      static_cast<double>(max_delay_frames_));
        const double whole = std::floor(delay);
        const auto whole_frames = static_cast<std::uint32_t>(whole);
        const LagrangeKernel kernel = lagrange_kernel(delay - whole, static_cast<double>(tap.gain));
        // The window never wraps past the ring's end; see modules/nodes/room/README.md.
        const std::uint32_t behind = whole_frames + 2u;
        const std::uint32_t start =
            block_start_ >= behind ? block_start_ - behind : block_start_ + ring_length_ - behind;
        const float* const __restrict source = ring_.data() + start;
        float* const __restrict destination = scratch_.data();
        const float c0 = kernel.c0;
        const float c1 = kernel.c1;
        const float c2 = kernel.c2;
        const float c3 = kernel.c3;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            destination[frame] = c0 * source[frame] + c1 * source[frame + 1u] +
                                 c2 * source[frame + 2u] + c3 * source[frame + 3u];
        }
    }

    void EarlyReflectionRenderer::filter_tap(const EarlyTap& tap, double* state,
                                             std::uint32_t frames) noexcept
    {
        const double b0a = tap.section_a[0];
        const double b1a = tap.section_a[1];
        const double b2a = tap.section_a[2];
        const double a1a = tap.section_a[3];
        const double a2a = tap.section_a[4];
        const double b0b = tap.section_b[0];
        const double b1b = tap.section_b[1];
        const double b2b = tap.section_b[2];
        const double a1b = tap.section_b[3];
        const double a2b = tap.section_b[4];
        double xa1 = state[0];
        double xa2 = state[1];
        double ya1 = state[2];
        double ya2 = state[3];
        double xb1 = state[4];
        double xb2 = state[5];
        double yb1 = state[6];
        double yb2 = state[7];
        float* const __restrict samples = scratch_.data();
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const double x = static_cast<double>(samples[frame]);
            const double ya = b0a * x + b1a * xa1 + b2a * xa2 - a1a * ya1 - a2a * ya2;
            xa2 = xa1;
            xa1 = x;
            ya2 = ya1;
            ya1 = ya;
            const double yb = b0b * ya + b1b * xb1 + b2b * xb2 - a1b * yb1 - a2b * yb2;
            xb2 = xb1;
            xb1 = ya;
            yb2 = yb1;
            yb1 = yb;
            samples[frame] = static_cast<float>(yb);
        }
        state[0] = xa1;
        state[1] = xa2;
        state[2] = ya1;
        state[3] = ya2;
        state[4] = xb1;
        state[5] = xb2;
        state[6] = yb1;
        state[7] = yb2;
    }

    void EarlyReflectionRenderer::encode_tap(const EarlyTap& tap, float* const* outputs,
                                             std::uint32_t frames) noexcept
    {
        const float* const __restrict source = scratch_.data();
        for (std::uint32_t channel = 0; channel < K_OUTPUT_CHANNELS; ++channel)
        {
            const float gain = tap.encode[channel];
            if (gain == 0.0f)
            {
                continue;
            }
            float* const __restrict destination = outputs[channel];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                destination[frame] += gain * source[frame];
            }
        }
    }
} // namespace SushiDSP::room
