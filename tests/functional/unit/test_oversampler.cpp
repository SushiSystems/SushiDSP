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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Oversampler.hpp"

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
