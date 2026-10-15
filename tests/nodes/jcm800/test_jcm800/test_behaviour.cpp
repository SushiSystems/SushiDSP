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
 * @brief Tests the Jcm800 node's parameters, levels, distortion and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // unity_master_db is derived from K_MASTER_UNITY_GAIN. See tests/nodes/jcm800/test_jcm800/README.md.
    const float unity_master_db =
        -SushiDSP::gain_to_decibels(SushiDSP::Jcm800::K_MASTER_UNITY_GAIN);
} // namespace

TEST(Small_Jcm800, Jcm800ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(0.0f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(0.5f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
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

TEST(Small_Jcm800, Jcm800OutputNeverExceedsFullScaleUnderHotGainStaging)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(1.0f);
    node.set_master(12.0f);
    node.set_bass(1.0f);
    node.set_middle(1.0f);
    node.set_treble(1.0f);
    node.set_presence(1.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    for (int block = 0; block < 40; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_LE(std::fabs(sample), 1.0f);
    }
}

TEST(Small_Jcm800, Jcm800PortsAreMonoInMonoOut)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Small_Jcm800, Jcm800MasterAtMinimumProducesSilenceAfterSettling)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(-60.0f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(0.5f);

    std::vector<float> input(256, 0.3f);
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
        EXPECT_NEAR(sample, 0.0f, 1e-2f);
    }
}

TEST(Small_Jcm800, Jcm800PreampIncreasesOutputEnergy)
{
    auto measure_rms = [](float preamp) -> float
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{48000.0, 256, 1};
        node.prepare(info);
        node.set_preamp(preamp);
        node.set_master(0.0f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        double sum_sq = 0.0;
        std::size_t count = 0;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = 0.05f * std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0);
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);

            if (block >= 15)
            {
                for (float sample : output)
                {
                    sum_sq += static_cast<double>(sample) * sample;
                    ++count;
                }
            }
        }
        return static_cast<float>(std::sqrt(sum_sq / static_cast<double>(count)));
    };

    const float low_preamp_rms = measure_rms(0.1f);
    const float high_preamp_rms = measure_rms(1.0f);
    std::printf("[ measure ] preamp energy: %.6f at 0.1, %.6f at 1.0\n", low_preamp_rms,
                high_preamp_rms);

    // The preamp knob never costs level; 1 % is sag drift. See tests/nodes/jcm800/test_jcm800/README.md.
    EXPECT_GE(high_preamp_rms, low_preamp_rms * 0.99f);
}

TEST(Small_Jcm800, Jcm800OutputTracksInputLevelRatherThanPinningToTheSolverClamp)
{
    // The knob is read at 0.02; at zero there is no output. See tests/nodes/jcm800/test_jcm800/README.md.
    auto measure_peak = [](float input_amplitude) -> float
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{48000.0, 256, 1};
        node.prepare(info);
        node.set_preamp(0.02f);
        node.set_master(0.0f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        float peak = 0.0f;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] =
                    input_amplitude *
                    static_cast<float>(std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0));
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);

            if (block >= 15)
            {
                for (float sample : output)
                {
                    peak = std::max(peak, std::abs(sample));
                }
            }
        }
        return peak;
    };

    const float quiet_peak = measure_peak(0.01f);
    const float loud_peak = measure_peak(0.251f);
    std::printf("[ measure ] level tracking: %.6f at 0.01 in, %.6f at 0.251 in\n", quiet_peak,
                loud_peak);

    EXPECT_GT(loud_peak, quiet_peak * 2.0f);
}

TEST(Small_Jcm800, Jcm800DescriptorExposesEightParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.jcm800");
    ASSERT_EQ(descriptor.parameter_count, 8u);

    // The index order is the contract of hosts and presets. See tests/nodes/jcm800/test_jcm800/README.md.
    const char* expected_ids[8] = {"preamp", "master",   "bass",       "middle",
                                   "treble", "presence", "input_trim", "output"};
    for (std::size_t i = 0; i < 8; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, expected_ids[i]);
    }

    EXPECT_EQ(descriptor.parameters[1].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[6].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[7].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].max, 12.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].default_value, 0.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].min, -24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].max, 24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].max, 12.0f);
}

TEST(Small_Jcm800, Jcm800ToneChangeDoesNotRebuildFiltersOffTheAudioThread)
{
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
    node.process(context);
    ASSERT_FALSE(node.tone_stack_is_dirty());

    // The control thread stores a target and nothing else. See tests/nodes/jcm800/test_jcm800/README.md.
    node.set_parameter_for_control_thread("bass", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 1.0f);
    EXPECT_TRUE(node.tone_stack_is_dirty());

    // The 20 ms smoothing outlasts one block. See tests/nodes/jcm800/test_jcm800/README.md.
    node.process(context);
    EXPECT_TRUE(node.tone_stack_is_dirty());

    for (int block = 0; block < 40 && node.tone_stack_is_dirty(); ++block)
    {
        node.process(context);
    }
    EXPECT_FALSE(node.tone_stack_is_dirty());
}

TEST(Small_Jcm800, Jcm800IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::Jcm800 node;
    const float values[8] = {0.2f, -6.0f, 0.4f, 0.45f, 0.6f, 0.7f, 12.0f, -3.0f};
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        node.set_parameter_for_audio_thread(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Small_Jcm800, Jcm800DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::Jcm800 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_Jcm800, Jcm800InputTrimAndOutputScaleTheSignalInDecibels)
{
    auto measure_rms = [](float input_trim_db, float output_db) -> double
    {
        SushiDSP::Jcm800 node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
        node.set_preamp(0.6f);
        node.set_master(0.0f);
        node.set_input_trim(input_trim_db);
        node.set_output(output_db);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        double sum_sq = 0.0;
        std::size_t count = 0;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = 0.02f * static_cast<float>(
                                       std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0));
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);

            if (block >= 20)
            {
                for (float sample : output)
                {
                    sum_sq += static_cast<double>(sample) * sample;
                    ++count;
                }
            }
        }
        return std::sqrt(sum_sq / static_cast<double>(count));
    };

    // The output control is linear and post-cabinet, so -6 dB there is exactly half the level.
    const double reference = measure_rms(0.0f, 0.0f);
    const double halved = measure_rms(0.0f, -6.020599913f);
    EXPECT_NEAR(halved, reference * 0.5, reference * 0.01);

    // The trim sits ahead of the nonlinear chain. See tests/nodes/jcm800/test_jcm800/README.md.
    const double trimmed_up = measure_rms(6.0f, 0.0f);
    EXPECT_GT(trimmed_up, reference);
}

TEST(Small_Jcm800, Jcm800DescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
    SushiDSP::PrepareInfo info{48000.0, 256, 1};

    std::unique_ptr<SushiDSP::AudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

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

TEST(Small_Jcm800, Jcm800IsDcStableAcrossManyBlocksAtHighPreamp)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(1.0f);
    node.set_master(unity_master_db);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(1.0f);

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

TEST(Small_Jcm800, Jcm800ThdDoesNotFallAsPreampRises)
{
    // THD must not fall as the preamp knob rises. See tests/nodes/jcm800/test_jcm800/README.md.
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(220.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    const auto stimulus =
        SushiDSP::measure::sine_stimulus(f0, 48000.0, SushiDSP::measure::K_FFT_SIZE * 2, 0.2f);

    auto thd_at = [&](float preamp)
    {
        SushiDSP::Jcm800 node;
        node.set_preamp(preamp);
        node.set_master(0.0f);
        const auto rendered = SushiDSP::measure::render(node, stimulus, 48000.0, 256);
        return SushiDSP::measure::thd(rendered, f0, 48000.0).thd_percent;
    };

    const float preamps[] = {0.02f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    double highest_so_far = 0.0;
    for (float preamp : preamps)
    {
        const double thd = thd_at(preamp);
        std::printf("[ measure ] THD %.3f %% at preamp %.2f\n", thd, preamp);
        EXPECT_GT(thd + 1.0, highest_so_far) << "preamp " << preamp;
        highest_so_far = std::max(highest_so_far, thd);
    }

    // Guards the sweep above against being satisfied by a run of near-zero readings.
    EXPECT_GT(highest_so_far, 20.0);
}

TEST(Small_Jcm800, Jcm800PowerTubeBiasMismatchShapesTheEvenHarmonicContent)
{
    // The sign of the difference is pinned at 1e-5. See tests/nodes/jcm800/test_jcm800/README.md.
    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 220.0;

    auto second_harmonic_ratio_for = [&](bool mismatch_active)
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{48000.0, 256, 1};
        node.prepare(info);
        if (!mismatch_active)
        {
            node.set_power_tube_grid_bias_mismatch_volts(0.0f);
        }
        node.set_preamp(0.5f);
        node.set_master(unity_master_db);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        std::vector<float> output;
        double global_sample_index = 0.0;
        for (int block = 0; block < 120; ++block)
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

        // Only the settled last quarter is measured. See tests/nodes/jcm800/test_jcm800/README.md.
        const std::size_t settle_start = output.size() * 3 / 4;
        const std::size_t n = output.size() - settle_start;
        auto harmonic_amplitude = [&](int harmonic)
        {
            double sum_sin = 0.0, sum_cos = 0.0;
            for (std::size_t i = settle_start; i < output.size(); ++i)
            {
                const double phase = 2.0 * M_PI * frequency * harmonic * i / sample_rate;
                sum_sin += output[i] * std::sin(phase);
                sum_cos += output[i] * std::cos(phase);
            }
            return 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
        };
        return harmonic_amplitude(2) / harmonic_amplitude(1);
    };

    const double active_ratio = second_harmonic_ratio_for(true);
    const double zeroed_ratio = second_harmonic_ratio_for(false);
    std::printf("[ measure ] second harmonic %.9f active, %.9f zeroed\n", active_ratio,
                zeroed_ratio);

    EXPECT_LT(active_ratio - zeroed_ratio, -1e-5);
}

TEST(Small_Jcm800, Jcm800GenericParameterRoundTrip)
{
    SushiDSP::Jcm800 node;
    node.set_parameter_for_control_thread("preamp", 0.2f);
    node.set_parameter_for_control_thread("master", 0.3f);
    node.set_parameter_for_control_thread("bass", 0.4f);
    node.set_parameter_for_control_thread("middle", 0.5f);
    node.set_parameter_for_control_thread("treble", 0.6f);
    node.set_parameter_for_control_thread("presence", 0.7f);
    node.set_parameter_for_control_thread("input_trim", 6.0f);
    node.set_parameter_for_control_thread("output", -3.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("preamp"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("master"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.7f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), -3.0f);
}

TEST(Small_Jcm800, Jcm800UnknownParameterIsSafeNoOp)
{
    SushiDSP::Jcm800 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Small_Jcm800, Jcm800NeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_parameter_for_control_thread("preamp", 1.0f);
    node.set_parameter_for_control_thread("master", 12.0f);
    node.set_parameter_for_control_thread("output", 12.0f);

    std::vector<float> input(256), output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    bool clamped_at_least_once = false;
    for (int block = 0; block < 50; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float t = static_cast<float>(block * 256 + i) / 48000.0f;
            input[i] = std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * t);
        }
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
        for (float sample : output)
        {
            EXPECT_LE(std::fabs(sample), 1.0f);
            if (std::fabs(sample) == 1.0f)
            {
                clamped_at_least_once = true;
            }
        }
    }
    EXPECT_TRUE(clamped_at_least_once);
}

TEST(Small_Jcm800, Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS low-E sine comes out at about -6 dBFS. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
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

        // The first blocks hold the startup transient. See tests/nodes/jcm800/test_jcm800/README.md.
        if (block > 20)
        {
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }

    const float peak_decibels = 20.0f * std::log10(peak);
    EXPECT_GT(peak_decibels, -8.0f);
    EXPECT_LT(peak_decibels, -4.0f);
}

TEST(Small_Jcm800, Jcm800DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Small_Jcm800, Jcm800HasACleanEndAtMinimumGain)
{
    // The knob is read at 0.01 because 0.0 passes no signal. See tests/nodes/jcm800/test_jcm800/README.md.
    const double f0 =
        SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    const auto stimulus =
        SushiDSP::measure::sine_stimulus(f0, 48000.0, SushiDSP::measure::K_FFT_SIZE * 2, 0.01f);

    SushiDSP::Jcm800 node;
    node.set_preamp(0.01f);
    node.set_master(0.0f);
    const auto rendered = SushiDSP::measure::render(node, stimulus, 48000.0, 256);
    const double clean_thd = SushiDSP::measure::thd(rendered, f0, 48000.0).thd_percent;
    std::printf("[ measure ] clean end: THD %.3f %% at preamp 0.01, -40 dBFS\n", clean_thd);

    EXPECT_LT(clean_thd, 10.0);
}

TEST(Small_Jcm800, Jcm800GridProbesReportEveryStageInSignalOrder)
{
    // The probe hooks are pinned, not their figures. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    ASSERT_EQ(node.triode_stage_count(), 4u);
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
    EXPECT_EQ(node.grid_excursion_peak_volts(99), 0.0f);

    // Before any audio every stage rests at its bias point, so every probe reads zero.
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    for (int block = 0; block < 40; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample_index = static_cast<float>(block * 256 + i);
            input[static_cast<std::size_t>(i)] =
                0.251f *
                std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    // Every stage is in the signal path, so a signal that reaches V1a reaches all four.
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
}
