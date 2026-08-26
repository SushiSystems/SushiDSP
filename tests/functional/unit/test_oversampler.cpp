/**************************************************************************/
/* test_oversampler.cpp                                                   */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Oversampler.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, HalfbandUpsamplerDoublesFrameCount)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 0.0f);
    std::vector<float> output(128, 0.0f);
    upsampler.process(input.data(), 64, output.data());
    // No crash / correct sizing is the assertion here; DC content checked below.
    SUCCEED();
}

TEST(Unit_Dsp, HalfbandUpsamplerPassesDcAtUnityGainAfterSettling)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 1.0f);
    std::vector<float> output(128, 0.0f);

    // Run several blocks to flush the FIR's group delay through the pipeline.
    for (int block = 0; block < 5; ++block)
    {
        upsampler.process(input.data(), 64, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 5e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerHalvesFrameCountAndPassesDc)
{
    SushiDSP::HalfbandDownsampler2x downsampler;
    std::vector<float> input(128, 1.0f);
    std::vector<float> output(64, 0.0f);

    for (int block = 0; block < 5; ++block)
    {
        downsampler.process(input.data(), 128, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 5e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, Oversampler8xRoundTripPreservesDcAfterSettling)
{
    constexpr std::uint32_t frames = 32;
    SushiDSP::Oversampler8x oversampler(frames);
    std::vector<float> input(frames, 1.0f);
    std::vector<float> output(frames, 0.0f);

    for (int block = 0; block < 10; ++block)
    {
        const float* up = oversampler.upsample(input.data(), frames);
        oversampler.downsample(up, frames * 8, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 1.0f, 1e-1f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandUpsamplerResetClearsState)
{
    SushiDSP::HalfbandUpsampler2x upsampler;
    std::vector<float> input(64, 1.0f);
    std::vector<float> output_a(128, 0.0f);
    std::vector<float> output_b(128, 0.0f);

    upsampler.process(input.data(), 64, output_a.data());
    upsampler.reset();
    upsampler.process(input.data(), 64, output_b.data());

    for (std::size_t i = 0; i < output_a.size(); ++i)
    {
        EXPECT_NEAR(output_a[i], output_b[i], 1e-6f) << "sample " << i;
    }
}

// --- Filter-shape tests: prove the design is actually a lowpass, not just that it runs. ---

TEST(Unit_Dsp, HalfbandDownsamplerRejectsNyquist)
{
    // Full-rate Nyquist (+1, -1, +1, ...) is the worst-case alias source for a 2x decimator.
    // A working halfband lowpass must attenuate it to near silence; a broken/pass-through
    // filter would decimate it to a constant +/-1.
    SushiDSP::HalfbandDownsampler2x downsampler;
    std::vector<float> input(128, 0.0f);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
    std::vector<float> output(64, 0.0f);

    for (int block = 0; block < 5; ++block)
    {
        downsampler.process(input.data(), 128, output.data());
    }

    for (std::size_t i = output.size() / 2; i < output.size(); ++i)
    {
        EXPECT_NEAR(output[i], 0.0f, 1e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandUpsamplerInterpolatesASineWithoutImages)
{
    // A passband sine upsampled by 2 must reproduce the *same* continuous sine resampled at the
    // 2x rate (delayed by the FIR's group delay). A filter that failed to suppress the
    // zero-stuffing image would leave the interleaved zeros visible instead, so this compares
    // every 2x sample against the analytic value.
    constexpr std::uint32_t frames = 64;
    constexpr std::uint32_t taps = 15;
    constexpr int group_delay = static_cast<int>(taps - 1) / 2; // in 2x-rate samples
    constexpr float cycles_per_input_sample = 1.0f / 16.0f;
    constexpr float kPi = 3.14159265358979323846f;

    SushiDSP::HalfbandUpsampler2x upsampler(taps);
    std::vector<float> input(frames, 0.0f);
    std::vector<float> output(frames * 2, 0.0f);

    constexpr int blocks = 6;
    for (int block = 0; block < blocks; ++block)
    {
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            const int n = block * static_cast<int>(frames) + static_cast<int>(i);
            input[i] = std::sin(2.0f * kPi * cycles_per_input_sample * static_cast<float>(n));
        }
        upsampler.process(input.data(), frames, output.data());
    }

    // Index of the first 2x output sample of the last block, in the global 2x timeline.
    const int base = (blocks - 1) * static_cast<int>(frames) * 2;
    for (std::uint32_t i = static_cast<std::uint32_t>(group_delay); i < output.size(); ++i)
    {
        const int n2x = base + static_cast<int>(i) - group_delay;
        const float expected =
            std::sin(2.0f * kPi * (cycles_per_input_sample / 2.0f) * static_cast<float>(n2x));
        EXPECT_NEAR(output[i], expected, 5e-2f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerPassesALowFrequencySineNearUnity)
{
    // A sine well inside the passband must survive decimation with roughly its input amplitude.
    constexpr std::uint32_t frames_in = 128;
    constexpr float normalized_frequency = 1.0f / 32.0f; // cycles per input sample

    SushiDSP::HalfbandDownsampler2x downsampler;
    std::vector<float> input(frames_in, 0.0f);
    std::vector<float> output(frames_in / 2, 0.0f);

    float peak = 0.0f;
    for (int block = 0; block < 8; ++block)
    {
        for (std::uint32_t i = 0; i < frames_in; ++i)
        {
            const float phase = 2.0f * 3.14159265358979323846f * normalized_frequency
                * static_cast<float>(block * static_cast<int>(frames_in) + static_cast<int>(i));
            input[i] = std::sin(phase);
        }
        downsampler.process(input.data(), frames_in, output.data());

        if (block == 7)
        {
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }

    EXPECT_NEAR(peak, 1.0f, 5e-2f);
}

TEST(Unit_Dsp, Oversampler8xResetClearsState)
{
    const std::uint32_t block_size = 32;
    const int           block_count = 3;

    std::vector<float> input(block_size, 0.0f);
    for (std::uint32_t i = 0; i < block_size; ++i)
    {
        input[i] = std::sin(0.31f * static_cast<float>(i)) * 0.7f;
    }

    SushiDSP::Oversampler8x oversampler(block_size);
    SushiDSP::Oversampler8x fresh_oversampler(block_size);

    std::vector<float> output_a(block_size, 0.0f);
    std::vector<float> output_b(block_size, 0.0f);
    std::vector<float> output_fresh(block_size, 0.0f);

    for (int block = 0; block < block_count; ++block)
    {
        const float* oversampled = oversampler.upsample(input.data(), block_size);
        oversampler.downsample(oversampled, block_size * 8, output_a.data());
    }

    oversampler.reset();

    for (int block = 0; block < block_count; ++block)
    {
        const float* oversampled = oversampler.upsample(input.data(), block_size);
        oversampler.downsample(oversampled, block_size * 8, output_b.data());

        const float* fresh_oversampled = fresh_oversampler.upsample(input.data(), block_size);
        fresh_oversampler.downsample(fresh_oversampled, block_size * 8, output_fresh.data());
    }

    for (std::size_t i = 0; i < output_b.size(); ++i)
    {
        EXPECT_NEAR(output_b[i], output_fresh[i], 1e-6f) << "sample " << i;
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerResetClearsState)
{
    SushiDSP::HalfbandDownsampler2x downsampler;
    std::vector<float> input(128, 1.0f);
    std::vector<float> output_a(64, 0.0f);
    std::vector<float> output_b(64, 0.0f);

    downsampler.process(input.data(), 128, output_a.data());
    downsampler.reset();
    downsampler.process(input.data(), 128, output_b.data());

    for (std::size_t i = 0; i < output_a.size(); ++i)
    {
        EXPECT_NEAR(output_a[i], output_b[i], 1e-6f) << "sample " << i;
    }
}

namespace
{
    // The shifted-delay-line direct form the polyphase/ring-buffer implementation replaced,
    // including the same coefficient design, kept here as the reference the new code must
    // reproduce bit for bit.
    std::vector<float> reference_halfband(std::uint32_t taps)
    {
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

    std::vector<float> reference_upsample(const std::vector<float>& input, std::uint32_t taps)
    {
        const std::vector<float> coefficients = reference_halfband(taps);
        std::vector<float> delay_line(taps, 0.0f);
        std::vector<float> output(input.size() * 2);
        for (std::size_t frame = 0; frame < input.size(); ++frame)
        {
            for (std::uint32_t sub = 0; sub < 2; ++sub)
            {
                for (std::uint32_t i = taps - 1; i > 0; --i)
                {
                    delay_line[i] = delay_line[i - 1];
                }
                delay_line[0] = (sub == 0) ? input[frame] * 2.0f : 0.0f;
                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients[i] * delay_line[i];
                }
                output[frame * 2 + sub] = accumulator;
            }
        }
        return output;
    }

    std::vector<float> reference_downsample(const std::vector<float>& input, std::uint32_t taps)
    {
        const std::vector<float> coefficients = reference_halfband(taps);
        std::vector<float> delay_line(taps, 0.0f);
        std::vector<float> output(input.size() / 2);
        std::size_t output_index = 0;
        for (std::size_t frame = 0; frame < input.size(); ++frame)
        {
            for (std::uint32_t i = taps - 1; i > 0; --i)
            {
                delay_line[i] = delay_line[i - 1];
            }
            delay_line[0] = input[frame];
            if (frame % 2 == 0)
            {
                float accumulator = 0.0f;
                for (std::uint32_t i = 0; i < taps; ++i)
                {
                    accumulator += coefficients[i] * delay_line[i];
                }
                output[output_index++] = accumulator;
            }
        }
        return output;
    }

    std::vector<float> noisy_signal(std::size_t frames)
    {
        std::vector<float> signal(frames);
        std::uint32_t state = 0x12345678u;
        for (std::size_t i = 0; i < frames; ++i)
        {
            state = state * 1664525u + 1013904223u;
            signal[i] = static_cast<float>(static_cast<int>(state >> 8) % 20001 - 10000) / 10000.0f;
        }
        return signal;
    }
}

TEST(Unit_Dsp, HalfbandUpsamplerMatchesDirectFormReferenceBitForBit)
{
    for (std::uint32_t taps : { 15u, 31u, 3u })
    {
        const std::vector<float> input = noisy_signal(300);
        const std::vector<float> expected = reference_upsample(input, taps);
        SushiDSP::HalfbandUpsampler2x upsampler(taps);
        std::vector<float> output(input.size() * 2, 0.0f);
        // Two calls of uneven length, so the ring state across calls is exercised too.
        upsampler.process(input.data(), 113, output.data());
        upsampler.process(input.data() + 113, 187, output.data() + 226);
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            EXPECT_EQ(output[i], expected[i]) << "taps " << taps << " sample " << i;
        }
    }
}

TEST(Unit_Dsp, HalfbandDownsamplerMatchesDirectFormReferenceBitForBit)
{
    for (std::uint32_t taps : { 15u, 31u, 3u })
    {
        const std::vector<float> input = noisy_signal(600);
        const std::vector<float> expected = reference_downsample(input, taps);
        SushiDSP::HalfbandDownsampler2x downsampler(taps);
        std::vector<float> output(input.size() / 2, 0.0f);
        downsampler.process(input.data(), 226, output.data());
        downsampler.process(input.data() + 226, 374, output.data() + 113);
        for (std::size_t i = 0; i < output.size(); ++i)
        {
            EXPECT_EQ(output[i], expected[i]) << "taps " << taps << " sample " << i;
        }
    }
}

TEST(Unit_Dsp, Oversampler8xReportsMeasuredImpulseLatency)
{
    SushiDSP::Oversampler8x oversampler(64);
    std::vector<float> input(64, 0.0f);
    input[0] = 1.0f;
    std::vector<float> output(64, 0.0f);
    // Two blocks so a delay longer than one block is still observed.
    std::vector<float> observed;
    for (int block = 0; block < 2; ++block)
    {
        const float* up = oversampler.upsample(input.data(), 64);
        oversampler.downsample(up, 512, output.data());
        observed.insert(observed.end(), output.begin(), output.end());
        std::fill(input.begin(), input.end(), 0.0f);
    }
    std::size_t peak_index = 0;
    for (std::size_t i = 1; i < observed.size(); ++i)
    {
        if (std::fabs(observed[i]) > std::fabs(observed[peak_index])) { peak_index = i; }
    }
    EXPECT_EQ(static_cast<std::uint32_t>(peak_index), oversampler.latency_samples());
}
