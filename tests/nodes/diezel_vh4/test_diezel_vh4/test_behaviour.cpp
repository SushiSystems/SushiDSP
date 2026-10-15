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
 * @brief Tests the DiezelVh4 node's parameters, levels and frequency response.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/diezel_vh4/diezel_vh4.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // unity_master_db is derived from the calibration constant. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const float unity_master_db =
        -SushiDSP::gain_to_decibels(SushiDSP::DiezelVh4::K_MASTER_UNITY_GAIN);

    /// @brief Reads one frequency out of a magnitude response.
    /// @param points The response frequency_response() returned.
    /// @param frequency_hz The frequency requested from it.
    /// @return That point's magnitude in decibels.
    double magnitude_at(const std::vector<SushiDSP::measure::ResponsePoint>& points,
                        double frequency_hz)
    {
        for (const SushiDSP::measure::ResponsePoint& point : points)
        {
            if (std::abs(point.frequency_hz - frequency_hz) < 1e-6)
            {
                return point.magnitude_db;
            }
        }
        return 0.0;
    }
} // namespace

TEST(Small_DiezelVh4, DiezelVh4ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_gain(0.6f);
    node.set_master(0.0f);

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

TEST(Small_DiezelVh4, DiezelVh4PortsAreMonoInMonoOut)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Small_DiezelVh4, DiezelVh4DescriptorExposesTenParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.diezel_vh4");
    EXPECT_STREQ(descriptor.name, "VH4 Ch3");
    ASSERT_EQ(descriptor.parameter_count, 10u);

    // The index order is the contract of hosts and presets. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const char* expected_ids[10] = {"gain", "bass",     "middle", "treble",     "volume",
                                    "deep", "presence", "master", "input_trim", "output"};
    for (std::size_t i = 0; i < 10; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, expected_ids[i]);
    }

    for (std::size_t i = 0; i < 7; ++i)
    {
        EXPECT_EQ(descriptor.parameters[i].kind, SushiDSP::ParamKind::Linear);
        EXPECT_FLOAT_EQ(descriptor.parameters[i].default_value, 0.5f);
    }
    EXPECT_EQ(descriptor.parameters[7].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[8].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[9].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].max, 12.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].default_value, 0.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[8].min, -24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[8].max, 24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[9].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[9].max, 12.0f);
}

TEST(Small_DiezelVh4, DiezelVh4IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::DiezelVh4 node;
    const float values[10] = {0.2f, 0.3f, 0.4f, 0.45f, 0.6f, 0.7f, 0.8f, -6.0f, 12.0f, -3.0f};
    for (std::uint16_t i = 0; i < 10; ++i)
    {
        node.set_parameter_for_audio_thread(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    for (std::uint16_t i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Small_DiezelVh4, DiezelVh4DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::DiezelVh4 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_DiezelVh4, DiezelVh4GenericParameterRoundTrip)
{
    SushiDSP::DiezelVh4 node;
    node.set_parameter_for_control_thread("gain", 0.2f);
    node.set_parameter_for_control_thread("bass", 0.3f);
    node.set_parameter_for_control_thread("middle", 0.4f);
    node.set_parameter_for_control_thread("treble", 0.5f);
    node.set_parameter_for_control_thread("volume", 0.6f);
    node.set_parameter_for_control_thread("deep", 0.7f);
    node.set_parameter_for_control_thread("presence", 0.8f);
    node.set_parameter_for_control_thread("master", -6.0f);
    node.set_parameter_for_control_thread("input_trim", 6.0f);
    node.set_parameter_for_control_thread("output", -3.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("gain"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("volume"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("deep"), 0.7f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.8f);
    EXPECT_FLOAT_EQ(node.get_parameter("master"), -6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), -3.0f);
}

TEST(Small_DiezelVh4, DiezelVh4UnknownParameterIsSafeNoOp)
{
    SushiDSP::DiezelVh4 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Small_DiezelVh4, DiezelVh4IsDcStableAcrossManyBlocksAtHighGain)
{
    // Each knob visits both extremes in turn: ten settings. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const char* const knobs[5] = {"gain", "bass", "middle", "treble", "volume"};
    for (const char* knob : knobs)
    {
        for (float extreme : {0.0f, 1.0f})
        {
            SushiDSP::DiezelVh4 node;
            node.set_parameter_for_control_thread(knob, extreme);
            node.set_master(unity_master_db);
            node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

            std::vector<float> input(256, 0.0f);
            std::vector<float> output(256, 0.0f);
            const float* input_channels[1] = {input.data()};
            float* output_channels[1] = {output.data()};

            double sum = 0.0;
            std::size_t counted = 0;
            for (int block = 0; block < 200; ++block)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const double t = static_cast<double>(block * 256 + i) / 48000.0;
                    input[static_cast<std::size_t>(i)] =
                        0.2f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 * t));
                }
                SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
                node.process(context);
                for (float sample : output)
                {
                    ASSERT_TRUE(std::isfinite(sample))
                        << knob << " at " << extreme << ", block " << block;
                    if (block >= 100)
                    {
                        sum += sample;
                        ++counted;
                    }
                }
            }

            // -40 dBFS of residual DC on the settled output.
            const double mean = sum / static_cast<double>(counted);
            EXPECT_LT(std::abs(mean), 0.01) << knob << " at " << extreme;
        }
    }
}

TEST(Small_DiezelVh4, DiezelVh4ThdDoesNotFallAsGainRises)
{
    // The THD reading must not fall as the gain knob rises. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    constexpr double sample_rate = 48000.0;
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(220.0, sample_rate, SushiDSP::measure::K_FFT_SIZE);

    auto thd_percent_for = [&](float gain) -> double
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(unity_master_db);
        const std::vector<float> stimulus =
            SushiDSP::measure::sine_stimulus(frequency, sample_rate, 131072, 0.2f);
        const std::vector<float> rendered =
            SushiDSP::measure::render(node, stimulus, sample_rate, 256);
        return SushiDSP::measure::thd(rendered, frequency, sample_rate).thd_percent;
    };

    const double low = thd_percent_for(0.1f);
    const double middle = thd_percent_for(0.5f);
    const double high = thd_percent_for(1.0f);
    std::printf("[ measure ] THD %.3f%% at gain 0.1, %.3f%% at 0.5, %.3f%% at 1.0\n", low, middle,
                high);

    constexpr double K_FALL_TOLERANCE_PERCENTAGE_POINTS = 0.5;
    EXPECT_GT(middle, low - K_FALL_TOLERANCE_PERCENTAGE_POINTS);
    EXPECT_GT(high, middle - K_FALL_TOLERANCE_PERCENTAGE_POINTS);
    EXPECT_GT(high, 10.0);
}

TEST(Small_DiezelVh4, DiezelVh4NeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_parameter_for_control_thread("gain", 1.0f);
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
            input[static_cast<std::size_t>(i)] =
                std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * t);
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

TEST(Small_DiezelVh4, DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
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

        // The first blocks hold the startup transient. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
        if (block > 20)
        {
            for (float sample : output)
            {
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }

    const float peak_decibels = 20.0f * std::log10(peak);
    std::printf("[ measure ] default peak %.3f dBFS\n", static_cast<double>(peak_decibels));
    EXPECT_GT(peak_decibels, -7.0f);
    EXPECT_LT(peak_decibels, -5.0f);
}

TEST(Small_DiezelVh4, DiezelVh4HasACleanEndAtMinimumGain)
{
    // The knob is read at 0.01 because 0.0 is a short. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    constexpr double sample_rate = 48000.0;
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(220.0, sample_rate, SushiDSP::measure::K_FFT_SIZE);

    SushiDSP::DiezelVh4 node;
    node.set_gain(0.01f);
    node.set_master(unity_master_db);
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(frequency, sample_rate, 131072, 0.01f); // -40 dBFS
    const std::vector<float> rendered = SushiDSP::measure::render(node, stimulus, sample_rate, 256);
    const double thd_percent = SushiDSP::measure::thd(rendered, frequency, sample_rate).thd_percent;

    std::printf("[ measure ] clean end THD %.4f%%\n", thd_percent);
    EXPECT_LT(thd_percent, 10.0);
}

TEST(Small_DiezelVh4, DiezelVh4GridProbesReportEveryStageInSignalOrder)
{
    // The probes cover all five stages in signal order. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    ASSERT_EQ(node.triode_stage_count(), std::size_t{5});

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
                std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
    EXPECT_EQ(node.grid_excursion_peak_volts(std::size_t{1000}), 0.0f);

    // reset() clears the peaks with the rest of the state.
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
}

TEST(Small_DiezelVh4, DiezelVh4DescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
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

TEST(Small_DiezelVh4, DiezelVh4DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Small_DiezelVh4, DiezelVh4GainKnobMovesTheInputHighPassCorner)
{
    // The knob's travel is worth 3.0 dB at 300 Hz. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    auto response_for = [](float gain)
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(unity_master_db);
        return SushiDSP::measure::frequency_response(node, 48000.0, {300.0, 1000.0});
    };

    const auto lowest = response_for(0.02f);
    const auto highest = response_for(1.0f);

    const double low_knob_tilt = magnitude_at(lowest, 300.0) - magnitude_at(lowest, 1000.0);
    const double high_knob_tilt = magnitude_at(highest, 300.0) - magnitude_at(highest, 1000.0);
    std::printf("[ measure ] 300 Hz relative to 1 kHz: %.3f dB at gain 0.02, %.3f dB at gain 1.0\n",
                low_knob_tilt, high_knob_tilt);

    EXPECT_GT(high_knob_tilt - low_knob_tilt, 2.5);
}

TEST(Small_DiezelVh4, DiezelVh4DeepAtMaximumIsATenDecibelShelfAtEightyHertz)
{
    // A shelf carries half its decibels at its corner. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    auto response_for = [](float deep)
    {
        SushiDSP::DiezelVh4 node;
        node.set_deep(deep);
        node.set_master(unity_master_db - 20.0f);
        return SushiDSP::measure::frequency_response(node, 48000.0, {20.0, 80.0});
    };

    const auto flat = response_for(0.0f);
    const auto deepest = response_for(1.0f);

    const double lift_at_eighty = magnitude_at(deepest, 80.0) - magnitude_at(flat, 80.0);
    const double lift_at_twenty = magnitude_at(deepest, 20.0) - magnitude_at(flat, 20.0);
    std::printf("[ measure ] Deep lift %.3f dB at 80 Hz, %.3f dB at 20 Hz\n", lift_at_eighty,
                lift_at_twenty);

    EXPECT_NEAR(lift_at_eighty, 5.0, 0.5);
}

TEST(Small_DiezelVh4, DiezelVh4MixBusAttenuationIsApplied)
{
    // The chain is linear end to end at -120 dBFS. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    constexpr double K_EXPECTED_SMALL_SIGNAL_GAIN_DB = 73.114;

    SushiDSP::DiezelVh4 node;
    node.set_gain(0.5f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_volume(0.5f);
    node.set_deep(0.0f);
    node.set_presence(0.5f);
    node.set_master(unity_master_db);

    constexpr double sample_rate = 48000.0;
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::K_FFT_SIZE);
    constexpr float amplitude = 0.000001f; // -120 dBFS
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(frequency, sample_rate, 131072, amplitude);
    const std::vector<float> rendered = SushiDSP::measure::render(node, stimulus, sample_rate, 256);

    float peak = 0.0f;
    for (std::size_t i = rendered.size() / 2; i < rendered.size(); ++i)
    {
        peak = std::max(peak, std::fabs(rendered[i]));
    }
    const double gain_db = 20.0 * std::log10(static_cast<double>(peak) / amplitude);
    std::printf("[ measure ] small-signal gain %.3f dB at 1 kHz\n", gain_db);

    EXPECT_NEAR(gain_db, K_EXPECTED_SMALL_SIGNAL_GAIN_DB, 0.5);
}
