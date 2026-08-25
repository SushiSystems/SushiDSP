/**************************************************************************/
/* test_tube_screamer.cpp                                                 */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, TubeScreamerProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.8f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, TubeScreamerPortsAreMonoInMonoOut)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, TubeScreamerLevelZeroProducesSilenceAfterSettling)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.0f);

    std::vector<float> input(256, 0.2f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-3f);
    }
}

TEST(Unit_Dsp, TubeScreamerDescriptorExposesDriveToneLevel)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.tubescreamer");
    ASSERT_EQ(descriptor.parameter_count, 3u);

    bool has_drive = false, has_tone = false, has_level = false;
    for (std::size_t i = 0; i < descriptor.parameter_count; ++i)
    {
        const std::string param_id = descriptor.parameters[i].id;
        if (param_id == "drive") has_drive = true;
        if (param_id == "tone") has_tone = true;
        if (param_id == "level") has_level = true;
    }
    EXPECT_TRUE(has_drive);
    EXPECT_TRUE(has_tone);
    EXPECT_TRUE(has_level);
}

TEST(Unit_Dsp, TubeScreamerDescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

    EXPECT_EQ(node->ports().input_channels, descriptor.ports.input_channels);
    EXPECT_EQ(node->ports().output_channels, descriptor.ports.output_channels);

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    node->process(context);

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, TubeScreamerIsDcStableAcrossManyBlocksAtHighDrive)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(1.0f);
    node.set_tone(0.5f);
    node.set_level(1.0f);

    std::vector<float> input(256, 0.1f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 200; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        for (float sample : output)
        {
            ASSERT_TRUE(std::isfinite(sample)) << "block " << block;
        }
    }
}

TEST(Unit_Dsp, TubeScreamerThdIncreasesWithDrive)
{
    auto measure_thd_proxy = [](float drive) -> float
    {
        SushiDSP::TubeScreamer node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_drive(drive);
        node.set_tone(0.5f);
        node.set_level(1.0f);

        constexpr float frequency = 220.0f;
        constexpr double sample_rate = 48000.0;
        std::vector<float> output;
        double global_sample_index = 0.0;

        for (int block = 0; block < 30; ++block)
        {
            std::vector<float> input(256, 0.0f);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = static_cast<float>(
                    0.2 * std::sin(2.0 * M_PI * frequency * (global_sample_index + i) / sample_rate));
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = { input.data() };
            float* output_channels[1] = { block_output.data() };
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);
            output.insert(output.end(), block_output.begin(), block_output.end());
        }

        // THD proxy: RMS deviation of the (settled, tail-half) output from a best-fit sine at
        // the fundamental, normalized by the output's own RMS. Not a true THD measurement (that
        // needs an FFT-based harmonic decomposition), but monotonic with clipping severity for
        // this purpose, and cheap.
        const std::size_t settle_start = output.size() / 2;
        double sum_sq = 0.0, sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = settle_start; i < output.size(); ++i)
        {
            const double phase = 2.0 * M_PI * frequency * i / sample_rate;
            sum_sq += static_cast<double>(output[i]) * output[i];
            sum_sin += output[i] * std::sin(phase);
            sum_cos += output[i] * std::cos(phase);
        }
        const std::size_t n = output.size() - settle_start;
        const double fundamental_amplitude = 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
        const double fundamental_power = 0.5 * fundamental_amplitude * fundamental_amplitude * n;
        const double residual_power = std::max(0.0, sum_sq - fundamental_power);
        return static_cast<float>(std::sqrt(residual_power / std::max(1e-12, sum_sq)));
    };

    const float thd_low_drive = measure_thd_proxy(0.05f);
    const float thd_high_drive = measure_thd_proxy(1.0f);

    EXPECT_LT(thd_low_drive, thd_high_drive);
}

TEST(Unit_Dsp, TubeScreamerToneControlsHighFrequencyBalance)
{
    auto measure_high_frequency_rms = [](float tone) -> double
    {
        SushiDSP::TubeScreamer node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_drive(0.2f);
        node.set_tone(tone);
        node.set_level(0.8f);

        SushiDSP::Biquad reference_highpass;
        reference_highpass.set_high_pass(48000.0, 2000.0, 0.707);

        constexpr double sample_rate = 48000.0;
        double global_sample_index = 0.0;
        double sum_squares = 0.0;
        std::size_t counted = 0;

        for (int block = 0; block < 40; ++block)
        {
            std::vector<float> input(256, 0.0f);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double t = (global_sample_index + i) / sample_rate;
                input[i] = static_cast<float>(
                    0.15 * std::sin(2.0 * M_PI * 100.0 * t)
                    + 0.15 * std::sin(2.0 * M_PI * 5000.0 * t));
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = { input.data() };
            float* output_channels[1] = { block_output.data() };
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);

            for (float sample : block_output)
            {
                const float filtered = reference_highpass.process_sample(sample);
                if (block >= 20)
                {
                    sum_squares += static_cast<double>(filtered) * filtered;
                    ++counted;
                }
            }
        }

        return std::sqrt(sum_squares / static_cast<double>(counted));
    };

    const double dark_high_frequency_rms = measure_high_frequency_rms(0.0f);
    const double bright_high_frequency_rms = measure_high_frequency_rms(1.0f);

    EXPECT_GT(bright_high_frequency_rms, dark_high_frequency_rms * 2.0);
}

TEST(Unit_Dsp, TubeScreamerGenericParameterRoundTrip)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter("drive", 0.25f);
    node.set_parameter("tone", 0.75f);
    node.set_parameter("level", 0.9f);

    EXPECT_FLOAT_EQ(node.get_parameter("drive"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("tone"), 0.75f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), 0.9f);
}

TEST(Unit_Dsp, TubeScreamerUnknownParameterIsSafeNoOp)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}
