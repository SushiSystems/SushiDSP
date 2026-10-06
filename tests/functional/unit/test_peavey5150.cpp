/**************************************************************************/
/* test_peavey5150.cpp                                                    */
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
 * @file test_peavey5150.cpp
 * @brief Tests the Peavey5150 node's parameters, levels, distortion and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/peavey5150/Peavey5150.hpp"
#include "measure/FrequencyResponse.hpp"
#include "measure/Render.hpp"
#include "measure/Thd.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr std::size_t kParameterCount = 9;

    /// @brief A node with every knob at its descriptor default, prepared at 48 kHz.
    std::unique_ptr<SushiDSP::Peavey5150> prepared_node(std::uint32_t block_size = 256)
    {
        auto node = std::make_unique<SushiDSP::Peavey5150>();
        node->prepare(SushiDSP::PrepareInfo{ 48000.0, block_size, 1 });
        return node;
    }

    // The shelves are read with a small-signal path ahead. See tests/functional/unit/README.md.
    constexpr float kLinearPathPreGain = 0.02f;
    constexpr float kLinearPathInputTrimDb = -24.0f;
    constexpr float kLinearPathPostGainDb = -40.0f;

    /// @brief A default node set to a level at which the chain ahead of the shelves stays linear.
    std::unique_ptr<SushiDSP::Peavey5150> small_signal_node()
    {
        auto node = prepared_node();
        node->set_pre_gain(kLinearPathPreGain);
        node->set_input_trim(kLinearPathInputTrimDb);
        node->set_post_gain(kLinearPathPostGainDb);
        return node;
    }

    /// @brief Runs a sine through a node and returns the settled tail of its output.
    std::vector<float> render_sine(SushiDSP::Peavey5150& node, double frequency, float amplitude, int blocks)
    {
        std::vector<float> input(256, 0.0f), block_output(256, 0.0f), output;
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { block_output.data() };
        double index = 0.0;
        for (int block = 0; block < blocks; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = static_cast<float>(
                    amplitude * std::sin(2.0 * M_PI * frequency * (index + static_cast<double>(i)) / 48000.0));
            }
            index += 256.0;
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);
            output.insert(output.end(), block_output.begin(), block_output.end());
        }
        return output;
    }

    // measure::thd() needs at least 65536 samples. See tests/functional/unit/README.md.
    constexpr int kSpectrumBlocks = 300;

    /// @brief The whole node's harmonic distortion at one pre-gain setting.
    /// @param pre_gain VR2's position.
    /// @param amplitude The stimulus amplitude.
    /// @return What measure::thd() returns for a 220 Hz tone through the settled node.
    SushiDSP::measure::ThdResult harmonics_at(float pre_gain, float amplitude)
    {
        auto node = prepared_node();
        node->set_pre_gain(pre_gain);
        return SushiDSP::measure::thd(render_sine(*node, 220.0, amplitude, kSpectrumBlocks), 220.0, 48000.0, 8);
    }

    /// @brief The magnitude one frequency comes back at, in decibels.
    double response_at(SushiDSP::Peavey5150& node, double frequency)
    {
        const auto points = SushiDSP::measure::frequency_response(node, 48000.0, { frequency });
        return points.front().magnitude_db;
    }
}

TEST(Unit_Dsp, Peavey5150ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    auto node = prepared_node();
    const std::vector<float> output = render_sine(*node, 220.0, 0.2f, 20);
    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, Peavey5150PortsAreMonoInMonoOut)
{
    SushiDSP::Peavey5150 node;
    const SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, Peavey5150DescriptorExposesNineParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.peavey5150");
    EXPECT_STREQ(descriptor.name, "5150");
    ASSERT_EQ(descriptor.parameter_count, kParameterCount);

    // The index order is the contract of hosts and presets. See tests/functional/unit/README.md.
    const char* expected_ids[kParameterCount] = {
        "pre_gain", "low", "mid", "high", "post_gain", "resonance", "presence", "input_trim", "output"
    };
    for (std::size_t i = 0; i < kParameterCount; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, expected_ids[i]);
    }

    EXPECT_EQ(descriptor.parameters[4].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[7].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[8].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_FLOAT_EQ(descriptor.parameters[4].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[4].max, 12.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[4].default_value, 0.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].min, -24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].max, 24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[8].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[8].max, 12.0f);
}

TEST(Unit_Dsp, Peavey5150IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::Peavey5150 node;
    const float values[kParameterCount] = { 0.2f, 0.4f, 0.45f, 0.6f, -6.0f, 0.3f, 0.7f, 12.0f, -3.0f };
    for (std::uint16_t i = 0; i < kParameterCount; ++i)
    {
        node.set_parameter_for_audio_thread(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();
    for (std::uint16_t i = 0; i < kParameterCount; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Unit_Dsp, Peavey5150DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::Peavey5150 node;
    node.set_post_gain(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("post_gain"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Unit_Dsp, Peavey5150GenericParameterRoundTrip)
{
    SushiDSP::Peavey5150 node;
    node.set_parameter_for_control_thread("pre_gain", 0.2f);
    node.set_parameter_for_control_thread("low", 0.4f);
    node.set_parameter_for_control_thread("mid", 0.5f);
    node.set_parameter_for_control_thread("high", 0.6f);
    node.set_parameter_for_control_thread("post_gain", -3.0f);
    node.set_parameter_for_control_thread("resonance", 0.3f);
    node.set_parameter_for_control_thread("presence", 0.7f);
    node.set_parameter_for_control_thread("input_trim", 6.0f);
    node.set_parameter_for_control_thread("output", -3.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("pre_gain"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("low"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("mid"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("high"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("post_gain"), -3.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("resonance"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.7f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), -3.0f);
}

TEST(Unit_Dsp, Peavey5150UnknownParameterIsSafeNoOp)
{
    SushiDSP::Peavey5150 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Unit_Dsp, Peavey5150ToneChangeDoesNotRebuildFiltersOffTheAudioThread)
{
    auto node = prepared_node();

    std::vector<float> input(256, 0.0f), output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    node->process(context);
    ASSERT_FALSE(node->tone_stack_is_dirty());

    // The control thread stores a target and nothing else. See tests/functional/unit/README.md.
    node->set_parameter_for_control_thread("low", 1.0f);
    EXPECT_TRUE(node->tone_stack_is_dirty());

    for (int block = 0; block < 40 && node->tone_stack_is_dirty(); ++block)
    {
        node->process(context);
    }
    EXPECT_FALSE(node->tone_stack_is_dirty());
}

TEST(Unit_Dsp, Peavey5150IsDcStableAcrossManyBlocksAtHighGain)
{
    // A stuck operating point shows as a drifting offset. See tests/functional/unit/README.md.
    for (float extreme : { 0.0f, 1.0f })
    {
        auto node = prepared_node();
        node->set_pre_gain(extreme);
        node->set_low(extreme);
        node->set_mid(extreme);
        node->set_high(extreme);
        node->set_resonance(extreme);
        node->set_presence(extreme);
        node->set_post_gain(extreme > 0.5f ? 12.0f : -60.0f);

        std::vector<float> input(256, 0.1f), output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };

        double sum = 0.0;
        std::size_t counted = 0;
        for (int block = 0; block < 200; ++block)
        {
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node->process(context);
            for (float sample : output)
            {
                ASSERT_TRUE(std::isfinite(sample)) << "extreme " << extreme << " block " << block;
                if (block >= 150)
                {
                    sum += sample;
                    ++counted;
                }
            }
        }
        const double mean = std::fabs(sum / static_cast<double>(counted));
        EXPECT_LT(20.0 * std::log10(std::max(mean, 1e-12)), -40.0) << "extreme " << extreme;
    }
}

TEST(Unit_Dsp, Peavey5150ThdDoesNotFallAsPreGainRises)
{
    // The sweep starts at 0.02, where the knob passes signal. See tests/functional/unit/README.md.
    const float positions[] = { 0.02f, 0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.7f, 1.0f };
    double previous = 0.0;
    bool first = true;
    for (float position : positions)
    {
        const double thd_percent = harmonics_at(position, 0.02f).thd_percent;
        std::printf("[ measure ] thd %.4f%% at pre gain %.2f\n", thd_percent, position);
        if (!first)
        {
            EXPECT_GT(thd_percent, previous - 1.0) << "pre gain " << position;
        }
        previous = thd_percent;
        first = false;
    }

    // Guards against the sweep above being satisfied by eight indistinguishable near-zero readings.
    EXPECT_GT(previous, 3.0);
}

TEST(Unit_Dsp, Peavey5150EvenHarmonicsRiseFromLowToFullPreGain)
{
    // The two points are drive levels, not knob positions. See tests/functional/unit/README.md.
    auto even_harmonic_ratio = [](float pre_gain)
    {
        const SushiDSP::measure::ThdResult result = harmonics_at(pre_gain, 0.02f);
        double even = 0.0;
        for (std::size_t harmonic = 2; harmonic <= 8; harmonic += 2)
        {
            even += std::pow(10.0, result.per_harmonic_db[harmonic - 1] / 10.0);
        }
        return std::sqrt(even);
    };

    constexpr float kQuietPreGain = 0.4776f;
    const double quiet = even_harmonic_ratio(kQuietPreGain);
    const double driven = even_harmonic_ratio(1.0f);
    std::printf("[ measure ] even harmonics %.5f at pre gain %.4f, %.5f at 1.0\n", quiet, kQuietPreGain, driven);

    // The gate is a factor of 2 against a measured 3.18. See tests/functional/unit/README.md.
    EXPECT_GT(driven, quiet * 2.0);
}

TEST(Unit_Dsp, Peavey5150GridProbesReportEveryStageInSignalOrder)
{
    // The probe hooks are pinned, not their figures. See tests/functional/unit/README.md.
    auto node = prepared_node();
    ASSERT_EQ(node->triode_stage_count(), std::size_t{ 8 });
    EXPECT_EQ(node->grid_excursion_peak_volts(node->triode_stage_count()), 0.0f);
    EXPECT_EQ(node->grid_excursion_peak_volts(99), 0.0f);

    // Before any audio every stage rests at its bias point, so every probe reads zero.
    for (std::size_t stage = 0; stage < node->triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node->grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    render_sine(*node, 1000.0, 0.251f, 40);
    for (std::size_t stage = 0; stage < node->triode_stage_count(); ++stage)
    {
        EXPECT_GT(node->grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    // reset() clears the peaks with the rest of the state.
    node->reset();
    for (std::size_t stage = 0; stage < node->triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node->grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
}

TEST(Unit_Dsp, Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels)
{
    // Presence at maximum lifts 2 kHz by 9.90 dB. See tests/functional/unit/README.md.
    auto flat = small_signal_node();
    flat->set_presence(0.5f);
    const double reference = response_at(*flat, 2000.0);

    auto lifted = small_signal_node();
    lifted->set_presence(1.0f);
    const double boosted = response_at(*lifted, 2000.0);

    std::printf("[ measure ] presence at 2 kHz: %.2f dB flat, %.2f dB full\n", reference, boosted);
    EXPECT_NEAR(boosted - reference, 10.0, 1.0);
}

TEST(Unit_Dsp, Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels)
{
    // Resonance at maximum lifts 100 Hz by 9.90 dB. See tests/functional/unit/README.md.
    auto flat = small_signal_node();
    flat->set_resonance(0.0f);
    const double reference = response_at(*flat, 100.0);

    auto lifted = small_signal_node();
    lifted->set_resonance(1.0f);
    const double boosted = response_at(*lifted, 100.0);

    std::printf("[ measure ] resonance at 100 Hz: %.2f dB flat, %.2f dB full\n", reference, boosted);
    EXPECT_NEAR(boosted - reference, 10.0, 1.0);
}

TEST(Unit_Dsp, Peavey5150NeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/functional/unit/README.md.
    auto node = prepared_node();
    node->set_parameter_for_control_thread("pre_gain", 1.0f);
    node->set_parameter_for_control_thread("post_gain", 12.0f);
    node->set_parameter_for_control_thread("output", 12.0f);

    const std::vector<float> output = render_sine(*node, 220.0, 1.0f, 50);
    bool clamped_at_least_once = false;
    for (float sample : output)
    {
        EXPECT_LE(std::fabs(sample), 1.0f);
        if (std::fabs(sample) == 1.0f)
        {
            clamped_at_least_once = true;
        }
    }
    EXPECT_TRUE(clamped_at_least_once);
}

TEST(Unit_Dsp, Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/functional/unit/README.md.
    auto node = prepared_node();

    std::vector<float> input(256, 0.0f), output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    float peak = 0.0f;
    for (int block = 0; block < 188; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample_index = static_cast<float>(block * 256 + i);
            input[static_cast<std::size_t>(i)] =
                0.251f * std::sin(2.0f * static_cast<float>(M_PI) * 82.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node->process(context);

        // The first blocks hold the startup transient. See tests/functional/unit/README.md.
        if (block > 20)
        {
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }

    const float peak_decibels = 20.0f * std::log10(peak);
    std::printf("[ measure ] default peak %.3f dBFS; kPostGainUnityGain would need x%.6f\n",
                static_cast<double>(peak_decibels),
                static_cast<double>(0.5011872f / std::max(peak, 1e-9f)));
    EXPECT_GT(peak_decibels, -7.0f);
    EXPECT_LT(peak_decibels, -5.0f);
}

TEST(Unit_Dsp, Peavey5150HasACleanEndAtMinimumGain)
{
    // The nominal lead input is -83.01 dBFS. See tests/functional/unit/README.md.
    constexpr float kLowestTestedPosition = 0.02f;
    constexpr float kMinusFortyDbfs = 0.01f;
    constexpr float kNominalMinusEightyDbv = 7.071e-5f;

    auto nominal = prepared_node();
    nominal->set_pre_gain(kLowestTestedPosition);
    const double nominal_thd =
        SushiDSP::measure::thd(render_sine(*nominal, 220.0, kNominalMinusEightyDbv, kSpectrumBlocks),
                               220.0, 48000.0, 8)
            .thd_percent;
    std::printf("[ measure ] thd %.4f%% at pre gain 0.02, -83.01 dBFS (Peavey's -80 dBV)\n",
                nominal_thd);
    EXPECT_LT(nominal_thd, 5.0);

    // Recorded, not asserted: a guitar-level input is 46 dB past what this channel is specified for.
    auto guitar_level = prepared_node();
    guitar_level->set_pre_gain(kLowestTestedPosition);
    const double guitar_level_thd =
        SushiDSP::measure::thd(render_sine(*guitar_level, 220.0, kMinusFortyDbfs, kSpectrumBlocks),
                               220.0, 48000.0, 8)
            .thd_percent;
    std::printf("[ measure ] thd %.4f%% at pre gain 0.02, -40 dBFS (recorded, not asserted)\n",
                guitar_level_thd);
}

TEST(Unit_Dsp, Peavey5150DescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();
    const SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

    std::vector<float> input(256, 0.0f), output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    node->process(context);

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, Peavey5150DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/functional/unit/README.md.
    auto node = prepared_node();

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node->latency_samples(), reference.latency_samples());
}
