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

#if defined(__AVX__)
#include <immintrin.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>

#include "SushiDSP/core/denormal_guard.hpp"

namespace SushiDSP::room
{
    void EarlyReflectionRenderer::prepare(std::uint32_t max_delay_frames,
                                          std::uint32_t max_block_frames)
    {
        max_delay_frames_ = max_delay_frames;
        max_block_frames_ = max_block_frames;
        scratch_.assign(static_cast<std::size_t>(K_LANES) * max_block_frames, 0.0f);
        filtered_.assign(static_cast<std::size_t>(K_LANES) * max_block_frames, 0.0f);
        state_.assign(static_cast<std::size_t>(K_BANK_COUNT) * EarlyTapSet::K_MAX_EARLY_TAPS *
                          K_STATE_PER_TAP,
                      0.0);
    }

    void EarlyReflectionRenderer::reset() noexcept
    {
        std::fill(state_.begin(), state_.end(), 0.0);
    }

    void EarlyReflectionRenderer::render(const DelayRing& ring, const EarlyTapSet& set,
                                         std::uint32_t bank, float* const* outputs,
                                         std::uint32_t frames) noexcept
    {
        const DenormalGuard guard;
        frames = std::min(frames, max_block_frames_);
        const std::uint32_t count = std::min<std::uint32_t>(
            set.count, static_cast<std::uint32_t>(EarlyTapSet::K_MAX_EARLY_TAPS));
        double* const state = bank_state(bank);
        for (std::uint32_t first = 0; first < count; first += K_LANES)
        {
            const std::uint32_t lanes = std::min(K_LANES, count - first);
            const EarlyTap* const taps = &set.taps[first];
            for (std::uint32_t lane = 0; lane < lanes; ++lane)
            {
                read_tap(ring, taps[lane], lane, frames);
            }
            filter_group(taps, lanes, state + static_cast<std::size_t>(first) * K_STATE_PER_TAP,
                         frames);
            for (std::uint32_t lane = 0; lane < lanes; ++lane)
            {
                encode_tap(taps[lane], lane, outputs, frames);
            }
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

    void EarlyReflectionRenderer::read_tap(const DelayRing& ring, const EarlyTap& tap,
                                           std::uint32_t lane, std::uint32_t frames) noexcept
    {
        const double delay = std::min(std::max(static_cast<double>(tap.delay_frames), 1.0),
                                      static_cast<double>(max_delay_frames_));
        ring.read(delay, tap.gain,
                  scratch_.data() + static_cast<std::size_t>(lane) * max_block_frames_, frames);
    }

    void EarlyReflectionRenderer::filter_group(const EarlyTap* taps, std::uint32_t lanes,
                                               double* state, std::uint32_t frames) noexcept
    {
        const std::size_t stride = max_block_frames_;
        const float* const input = scratch_.data();
        float* const output = filtered_.data();
#if defined(__AVX__)
        // A lane past the group keeps zero coefficients and zero state, and writes zeros.
        alignas(32) double coefficient[10][K_LANES] = {};
        alignas(32) double history[K_STATE_PER_TAP][K_LANES] = {};
        for (std::uint32_t lane = 0; lane < lanes; ++lane)
        {
            for (std::size_t index = 0; index < 5; ++index)
            {
                coefficient[index][lane] = taps[lane].section_a[index];
                coefficient[5 + index][lane] = taps[lane].section_b[index];
            }
            for (std::size_t index = 0; index < K_STATE_PER_TAP; ++index)
            {
                history[index][lane] = state[lane * K_STATE_PER_TAP + index];
            }
        }
        const __m256d b0a = _mm256_load_pd(coefficient[0]);
        const __m256d b1a = _mm256_load_pd(coefficient[1]);
        const __m256d b2a = _mm256_load_pd(coefficient[2]);
        const __m256d a1a = _mm256_load_pd(coefficient[3]);
        const __m256d a2a = _mm256_load_pd(coefficient[4]);
        const __m256d b0b = _mm256_load_pd(coefficient[5]);
        const __m256d b1b = _mm256_load_pd(coefficient[6]);
        const __m256d b2b = _mm256_load_pd(coefficient[7]);
        const __m256d a1b = _mm256_load_pd(coefficient[8]);
        const __m256d a2b = _mm256_load_pd(coefficient[9]);
        __m256d xa1 = _mm256_load_pd(history[0]);
        __m256d xa2 = _mm256_load_pd(history[1]);
        __m256d ya1 = _mm256_load_pd(history[2]);
        __m256d ya2 = _mm256_load_pd(history[3]);
        __m256d xb1 = _mm256_load_pd(history[4]);
        __m256d xb2 = _mm256_load_pd(history[5]);
        __m256d yb1 = _mm256_load_pd(history[6]);
        __m256d yb2 = _mm256_load_pd(history[7]);
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            const __m256d x = _mm256_cvtps_pd(_mm_setr_ps(input[frame], input[stride + frame],
                                                         input[2 * stride + frame],
                                                         input[3 * stride + frame]));
            // The terms are added in the order the scalar section adds them, lane by lane.
            const __m256d ya = _mm256_sub_pd(
                _mm256_sub_pd(_mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(b0a, x),
                                                          _mm256_mul_pd(b1a, xa1)),
                                            _mm256_mul_pd(b2a, xa2)),
                              _mm256_mul_pd(a1a, ya1)),
                _mm256_mul_pd(a2a, ya2));
            xa2 = xa1;
            xa1 = x;
            ya2 = ya1;
            ya1 = ya;
            const __m256d yb = _mm256_sub_pd(
                _mm256_sub_pd(_mm256_add_pd(_mm256_add_pd(_mm256_mul_pd(b0b, ya),
                                                          _mm256_mul_pd(b1b, xb1)),
                                            _mm256_mul_pd(b2b, xb2)),
                              _mm256_mul_pd(a1b, yb1)),
                _mm256_mul_pd(a2b, yb2));
            xb2 = xb1;
            xb1 = ya;
            yb2 = yb1;
            yb1 = yb;
            alignas(16) float narrowed[K_LANES];
            _mm_store_ps(narrowed, _mm256_cvtpd_ps(yb));
            output[frame] = narrowed[0];
            output[stride + frame] = narrowed[1];
            output[2 * stride + frame] = narrowed[2];
            output[3 * stride + frame] = narrowed[3];
        }
        _mm256_store_pd(history[0], xa1);
        _mm256_store_pd(history[1], xa2);
        _mm256_store_pd(history[2], ya1);
        _mm256_store_pd(history[3], ya2);
        _mm256_store_pd(history[4], xb1);
        _mm256_store_pd(history[5], xb2);
        _mm256_store_pd(history[6], yb1);
        _mm256_store_pd(history[7], yb2);
        for (std::uint32_t lane = 0; lane < lanes; ++lane)
        {
            for (std::size_t index = 0; index < K_STATE_PER_TAP; ++index)
            {
                state[lane * K_STATE_PER_TAP + index] = history[index][lane];
            }
        }
#else
        for (std::uint32_t lane = 0; lane < lanes; ++lane)
        {
            const std::array<double, 5>& a = taps[lane].section_a;
            const std::array<double, 5>& b = taps[lane].section_b;
            double* const history = state + lane * K_STATE_PER_TAP;
            double xa1 = history[0];
            double xa2 = history[1];
            double ya1 = history[2];
            double ya2 = history[3];
            double xb1 = history[4];
            double xb2 = history[5];
            double yb1 = history[6];
            double yb2 = history[7];
            const float* const __restrict from = input + lane * stride;
            float* const __restrict to = output + lane * stride;
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                const double x = static_cast<double>(from[frame]);
                const double ya = a[0] * x + a[1] * xa1 + a[2] * xa2 - a[3] * ya1 - a[4] * ya2;
                xa2 = xa1;
                xa1 = x;
                ya2 = ya1;
                ya1 = ya;
                const double yb = b[0] * ya + b[1] * xb1 + b[2] * xb2 - b[3] * yb1 - b[4] * yb2;
                xb2 = xb1;
                xb1 = ya;
                yb2 = yb1;
                yb1 = yb;
                to[frame] = static_cast<float>(yb);
            }
            history[0] = xa1;
            history[1] = xa2;
            history[2] = ya1;
            history[3] = ya2;
            history[4] = xb1;
            history[5] = xb2;
            history[6] = yb1;
            history[7] = yb2;
        }
#endif
    }

    void EarlyReflectionRenderer::encode_tap(const EarlyTap& tap, std::uint32_t lane,
                                             float* const* outputs,
                                             std::uint32_t frames) noexcept
    {
        const float* const __restrict source =
            filtered_.data() + static_cast<std::size_t>(lane) * max_block_frames_;
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
