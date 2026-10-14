/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests the TubeScreamer node's parameters, levels, smoothing and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/biquad.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, TubeScreamerProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
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

TEST(Unit_Dsp, TubeScreamerMinimumLevelProducesNearSilenceAfterSettling)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(-60.0f);

    std::vector<float> input(256, 0.2f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    for (int block = 0; block < 20; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_NEAR(sample, 0.0f, 1e-3f);
    }
}

TEST(Unit_Dsp, TubeScreamerDescriptorExposesFourParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.tubescreamer");
    ASSERT_EQ(descriptor.parameter_count, 4u);
    EXPECT_STREQ(descriptor.parameters[0].id, "drive");
    EXPECT_STREQ(descriptor.parameters[1].id, "tone");
    EXPECT_STREQ(descriptor.parameters[2].id, "level");
    EXPECT_EQ(descriptor.parameters[2].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_STREQ(descriptor.parameters[3].id, "input_trim");
    EXPECT_EQ(descriptor.parameters[3].kind, SushiDSP::ParamKind::Decibel);
}

TEST(Unit_Dsp, TubeScreamerIndexSetParameterMatchesStringSetParameter)
{
    SushiDSP::TubeScreamer by_name, by_index;

    by_name.set_parameter_for_control_thread("tone", 0.8f);
    by_index.set_parameter_for_audio_thread(static_cast<std::uint16_t>(1), 0.8f);
    EXPECT_FLOAT_EQ(by_name.get_parameter("tone"), by_index.get_parameter("tone"));

    by_name.set_parameter_for_control_thread("input_trim", -6.0f);
    by_index.set_parameter_for_audio_thread(static_cast<std::uint16_t>(3), -6.0f);
    EXPECT_FLOAT_EQ(by_name.get_parameter("input_trim"), by_index.get_parameter("input_trim"));
}

TEST(Unit_Dsp, TubeScreamerLevelAndInputTrimClampToTheirDescriptorRange)
{
    SushiDSP::TubeScreamer node;

    node.set_level(99.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), 12.0f);
    node.set_level(-200.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), -60.0f);
    node.set_input_trim(99.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 24.0f);
    node.set_input_trim(-99.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
}

TEST(Unit_Dsp, TubeScreamerInputTrimScalesTheSignalIntoTheClipper)
{
    auto settled_peak_at_trim = [](float trim_decibels)
    {
        SushiDSP::TubeScreamer node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
        node.set_drive(0.0f);
        node.set_tone(0.5f);
        node.set_level(0.0f);
        node.set_input_trim(trim_decibels);

        std::vector<float> input(256, 0.0f), output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        double index = 0.0;
        double peak = 0.0;

        for (int block = 0; block < 40; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] =
                    static_cast<float>(0.02 * std::sin(2.0 * M_PI * 440.0 * (index + i) / 48000.0));
            }
            index += 256.0;
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);
            if (block >= 20)
            {
                for (float sample : output)
                {
                    peak = std::max(peak, std::fabs(static_cast<double>(sample)));
                }
            }
        }
        return peak;
    };

    const double unity = settled_peak_at_trim(0.0f);
    EXPECT_GT(settled_peak_at_trim(12.0f), unity * 2.0);
    EXPECT_LT(settled_peak_at_trim(-12.0f), unity * 0.5);
}

TEST(Unit_Dsp, TubeScreamerSmoothingTimeIsBlockSizeIndependent)
{
    // The 63 % crossing must not depend on the block size. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    constexpr double sample_rate = 48000.0;
    constexpr double probe_hz = 4000.0;
    constexpr double probe_amplitude = 0.02;

    auto crossing_ms = [&](std::uint32_t block)
    {
        std::vector<float> input(block, 0.0f), output(block, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        double index = 0.0;

        auto run_block = [&](SushiDSP::TubeScreamer& node)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = static_cast<float>(
                    probe_amplitude * std::sin(2.0 * M_PI * probe_hz * (index + i) / sample_rate));
            }
            index += block;
            SushiDSP::ProcessContext context{output_channels, input_channels, block, 0.0};
            node.process(context);
        };
        auto block_peak = [&]()
        {
            double peak = 0.0;
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(static_cast<double>(sample)));
            }
            return peak;
        };

        const std::uint32_t one_second_of_blocks = (48000u + block - 1u) / block;

        SushiDSP::TubeScreamer settled;
        settled.prepare(SushiDSP::PrepareInfo{sample_rate, block, 1});
        settled.set_parameter_for_control_thread("level", 0.0f);
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            run_block(settled);
        }
        const double final_peak = block_peak();

        SushiDSP::TubeScreamer stepping;
        stepping.prepare(SushiDSP::PrepareInfo{sample_rate, block, 1});
        stepping.set_parameter_for_control_thread("level", -60.0f);
        index = 0.0;
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            run_block(stepping);
        }

        stepping.set_parameter_for_control_thread("level", 0.0f);
        double elapsed_samples = 0.0;
        for (std::uint32_t i = 0; i < one_second_of_blocks; ++i)
        {
            run_block(stepping);
            for (std::uint32_t sample = 0; sample < block; ++sample)
            {
                if (std::fabs(static_cast<double>(output[sample])) > 0.63 * final_peak)
                {
                    return (elapsed_samples + sample) / (sample_rate / 1000.0);
                }
            }
            elapsed_samples += block;
        }
        return -1.0;
    };

    const double at_64 = crossing_ms(64);
    const double at_4096 = crossing_ms(4096);
    ASSERT_GT(at_64, 0.0);
    ASSERT_GT(at_4096, 0.0);
    EXPECT_NEAR(at_64, at_4096, 0.05 * at_64);
}

TEST(Unit_Dsp, TubeScreamerReportsItsOversamplerLatencyAndTheBlocksAddOnlyPhaseLag)
{
    // Only the 4x oversampler's round trip is latency. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    static constexpr std::uint32_t block = 64;
    constexpr std::uint32_t max_block_group_delay = 4;

    auto peak_index_at_tone = [](float tone)
    {
        SushiDSP::TubeScreamer node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, block, 1});
        node.set_drive(0.0f);
        node.set_tone(tone);
        node.set_level(0.0f);

        std::vector<float> input(block, 0.0f), output(block, 0.0f);
        input[0] = 0.001f;
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        // Two blocks so a delay longer than one block would still be observed.
        std::vector<float> observed;
        for (int b = 0; b < 2; ++b)
        {
            SushiDSP::ProcessContext context{output_channels, input_channels, block, 0.0};
            node.process(context);
            observed.insert(observed.end(), output.begin(), output.end());
            std::fill(input.begin(), input.end(), 0.0f);
        }

        std::size_t peak_index = 0;
        for (std::size_t i = 1; i < observed.size(); ++i)
        {
            if (std::fabs(observed[i]) > std::fabs(observed[peak_index]))
            {
                peak_index = i;
            }
        }
        return std::make_pair(static_cast<std::uint32_t>(peak_index), node.latency_samples());
    };

    SushiDSP::TubeScreamer unprepared;
    EXPECT_EQ(unprepared.latency_samples(), 0u);

    for (float tone : {0.0f, 0.5f, 1.0f})
    {
        const auto measured = peak_index_at_tone(tone);
        EXPECT_GE(measured.first, measured.second) << "tone " << tone;
        EXPECT_LE(measured.first, measured.second + max_block_group_delay)
            << "tone " << tone << ", peak at " << measured.first << ", latency_samples() "
            << measured.second;
    }
}

TEST(Unit_Dsp, TubeScreamerDescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();
    SushiDSP::PrepareInfo info{48000.0, 256, 1};

    std::unique_ptr<SushiDSP::AudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

    EXPECT_EQ(node->ports().input_channels, descriptor.ports.input_channels);
    EXPECT_EQ(node->ports().output_channels, descriptor.ports.output_channels);

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
    node->process(context);

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, TubeScreamerIsDcStableAcrossManyBlocksAtHighDrive)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_drive(1.0f);
    node.set_tone(0.5f);
    node.set_level(0.0f);

    std::vector<float> input(256, 0.1f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    for (int block = 0; block < 200; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
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
        SushiDSP::PrepareInfo info{48000.0, 256, 1};
        node.prepare(info);
        node.set_drive(drive);
        node.set_tone(0.5f);
        node.set_level(0.0f);

        constexpr float frequency = 220.0f;
        constexpr double sample_rate = 48000.0;
        std::vector<float> output;
        double global_sample_index = 0.0;

        for (int block = 0; block < 30; ++block)
        {
            std::vector<float> input(256, 0.0f);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] =
                    static_cast<float>(0.2 * std::sin(2.0 * M_PI * frequency *
                                                      (global_sample_index + i) / sample_rate));
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = {input.data()};
            float* output_channels[1] = {block_output.data()};
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);
            output.insert(output.end(), block_output.begin(), block_output.end());
        }

        // The THD proxy is RMS deviation from a fitted sine. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
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
        const double fundamental_amplitude =
            2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
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
        SushiDSP::PrepareInfo info{48000.0, 256, 1};
        node.prepare(info);
        node.set_drive(0.2f);
        node.set_tone(tone);
        node.set_level(0.0f);

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
                input[i] = static_cast<float>(0.15 * std::sin(2.0 * M_PI * 100.0 * t) +
                                              0.15 * std::sin(2.0 * M_PI * 5000.0 * t));
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = {input.data()};
            float* output_channels[1] = {block_output.data()};
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
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
    node.set_parameter_for_control_thread("drive", 0.25f);
    node.set_parameter_for_control_thread("tone", 0.75f);
    node.set_parameter_for_control_thread("level", -3.0f);
    node.set_parameter_for_control_thread("input_trim", 6.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("drive"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("tone"), 0.75f);
    EXPECT_FLOAT_EQ(node.get_parameter("level"), -3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
}

TEST(Unit_Dsp, TubeScreamerUnknownParameterIsSafeNoOp)
{
    SushiDSP::TubeScreamer node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Unit_Dsp, TubeScreamerDefaultSettingsPeakNearMinusTwelveDbfsForAMinusTwelveDbfsInput)
{
    // At default drive the pedal is near unity at 82 Hz. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    SushiDSP::TubeScreamer node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    float peak = 0.0f;
    for (int block = 0; block < 188; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample_index = static_cast<float>(block * 256 + i);
            input[static_cast<std::size_t>(i)] = 0.251f * std::sin(2.0f * static_cast<float>(M_PI) *
                                                                   82.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);

        if (block > 20)
        {
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }

    const float peak_decibels = 20.0f * std::log10(peak);
    EXPECT_GT(peak_decibels, -14.0f) << "measured peak " << peak_decibels << " dBFS";
    EXPECT_LT(peak_decibels, -10.0f) << "measured peak " << peak_decibels << " dBFS";
}

TEST(Unit_Dsp, TubeScreamerHasNoCornerNearTwentyHertzTheOldBiquadRemoved)
{
    // The input corner is 0.312069 Hz, so 20 Hz is flat. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    auto settled_rms_at = [](double frequency)
    {
        SushiDSP::TubeScreamer node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
        node.set_drive(0.0f);
        node.set_tone(0.5f);
        node.set_level(0.0f);

        std::vector<float> input(256, 0.0f), output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        double index = 0.0, sum_squares = 0.0;
        std::size_t counted = 0;

        // The window is truncated to whole cycles. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
        const std::size_t cycle_samples = static_cast<std::size_t>(48000.0 / frequency);
        const std::size_t window = 25600 / cycle_samples * cycle_samples;

        for (int block = 0; block < 200; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = static_cast<float>(
                    0.01 * std::sin(2.0 * M_PI * frequency * (index + i) / 48000.0));
            }
            index += 256.0;
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);
            if (block >= 100)
            {
                for (float sample : output)
                {
                    if (counted == window)
                    {
                        break;
                    }
                    sum_squares += static_cast<double>(sample) * sample;
                    ++counted;
                }
            }
        }
        return std::sqrt(sum_squares / static_cast<double>(counted));
    };

    const double rms_at_30_hz = settled_rms_at(30.0);
    const double rms_at_10_hz = settled_rms_at(10.0);
    EXPECT_LT(rms_at_30_hz / rms_at_10_hz, 1.5)
        << "30 Hz " << rms_at_30_hz << ", 10 Hz " << rms_at_10_hz;
}

TEST(Unit_Dsp, TubeScreamerDriveKnobMovesTheClipThresholdWithoutClicking)
{
    // A drive sweep must not produce a sample-to-sample step. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    static constexpr std::uint32_t block = 256;
    static constexpr std::uint32_t blocks_per_second = 48000u / block;

    auto largest_first_difference = [](bool sweeping)
    {
        SushiDSP::TubeScreamer node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, block, 1});
        node.set_drive(sweeping ? 0.0f : 1.0f);
        node.set_tone(0.5f);
        node.set_level(0.0f);

        std::vector<float> input(block, 0.0f), output(block, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        double index = 0.0, largest = 0.0;
        float previous = 0.0f;

        for (std::uint32_t b = 0; b < blocks_per_second; ++b)
        {
            if (sweeping)
            {
                node.set_drive(static_cast<float>(b) / static_cast<float>(blocks_per_second - 1u));
            }
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] =
                    static_cast<float>(0.1 * std::sin(2.0 * M_PI * 220.0 * (index + i) / 48000.0));
            }
            index += block;
            SushiDSP::ProcessContext context{output_channels, input_channels, block, 0.0};
            node.process(context);

            for (float sample : output)
            {
                if (b >= 10)
                {
                    largest = std::max(largest, std::fabs(static_cast<double>(sample - previous)));
                }
                previous = sample;
            }
        }
        return largest;
    };

    const double still = largest_first_difference(false);
    const double swept = largest_first_difference(true);
    EXPECT_LT(swept, still * 2.0) << "swept " << swept << ", still " << still;
}
