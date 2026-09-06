/**************************************************************************/
/* test_diezel_vh4.cpp                                                    */
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
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/diezel_vh4/DiezelVh4.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "measure/FrequencyResponse.hpp"
#include "measure/Render.hpp"
#include "measure/Thd.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // The master level whose linear multiplier is exactly 1.0, derived from the calibration
    // constant rather than written out, so a recalibration does not silently move what the tests
    // below measure.
    const float kUnityMasterDb = -SushiDSP::gain_to_decibels(SushiDSP::DiezelVh4::kMasterUnityGain);

    /// @brief Reads one frequency out of a magnitude response.
    /// @param points The response frequency_response() returned.
    /// @param frequency_hz The frequency requested from it.
    /// @return That point's magnitude in decibels.
    double magnitude_at(const std::vector<SushiDSP::measure::ResponsePoint>& points, double frequency_hz)
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
}

TEST(Unit_Dsp, DiezelVh4ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_gain(0.6f);
    node.set_master(0.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
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

TEST(Unit_Dsp, DiezelVh4PortsAreMonoInMonoOut)
{
    SushiDSP::DiezelVh4 node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, DiezelVh4DescriptorExposesTenParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.diezel_vh4");
    EXPECT_STREQ(descriptor.name, "VH4 Ch3");
    ASSERT_EQ(descriptor.parameter_count, 10u);

    // The index order is the contract a host and a saved preset address these by, so it is pinned
    // here rather than merely checked for membership. The first five are the channel's own front
    // panel in panel order; Deep, Presence and Master are the master section.
    const char* expected_ids[10] = {
        "gain", "bass", "middle", "treble", "volume",
        "deep", "presence", "master", "input_trim", "output"
    };
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

TEST(Unit_Dsp, DiezelVh4IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::DiezelVh4 node;
    const float values[10] = { 0.2f, 0.3f, 0.4f, 0.45f, 0.6f, 0.7f, 0.8f, -6.0f, 12.0f, -3.0f };
    for (std::uint16_t i = 0; i < 10; ++i)
    {
        node.set_parameter(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    for (std::uint16_t i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Unit_Dsp, DiezelVh4DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::DiezelVh4 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Unit_Dsp, DiezelVh4GenericParameterRoundTrip)
{
    SushiDSP::DiezelVh4 node;
    node.set_parameter("gain", 0.2f);
    node.set_parameter("bass", 0.3f);
    node.set_parameter("middle", 0.4f);
    node.set_parameter("treble", 0.5f);
    node.set_parameter("volume", 0.6f);
    node.set_parameter("deep", 0.7f);
    node.set_parameter("presence", 0.8f);
    node.set_parameter("master", -6.0f);
    node.set_parameter("input_trim", 6.0f);
    node.set_parameter("output", -3.0f);

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

TEST(Unit_Dsp, DiezelVh4UnknownParameterIsSafeNoOp)
{
    SushiDSP::DiezelVh4 node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Unit_Dsp, DiezelVh4IsDcStableAcrossManyBlocksAtHighGain)
{
    // Every one of the five normalized knobs is taken to both of its extremes in turn, which is
    // ten settings rather than the 32 a full cross product would be: the failure this guards
    // against - a stage's warm start integrating a DC term until the output walks off - is a
    // property of one extreme at a time, and 32 x 200 blocks of an 8x oversampled six-tube chain
    // buys nothing for the runtime it costs.
    const char* const knobs[5] = { "gain", "bass", "middle", "treble", "volume" };
    for (const char* knob : knobs)
    {
        for (float extreme : { 0.0f, 1.0f })
        {
            SushiDSP::DiezelVh4 node;
            node.set_parameter(knob, extreme);
            node.set_master(kUnityMasterDb);
            node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

            std::vector<float> input(256, 0.0f);
            std::vector<float> output(256, 0.0f);
            const float* input_channels[1] = { input.data() };
            float* output_channels[1] = { output.data() };

            double sum = 0.0;
            std::size_t counted = 0;
            for (int block = 0; block < 200; ++block)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const double t = static_cast<double>(block * 256 + i) / 48000.0;
                    input[static_cast<std::size_t>(i)] = 0.2f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 * t));
                }
                SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
                node.process(context);
                for (float sample : output)
                {
                    ASSERT_TRUE(std::isfinite(sample)) << knob << " at " << extreme << ", block " << block;
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

TEST(Unit_Dsp, DiezelVh4ThdIncreasesWithGain)
{
    // The gain pot is this channel's only drive control, so it has to sweep the cascade from
    // nearly clean into saturation. Measured with measure::thd() at a bin-exact 220 Hz on the
    // settled tail of a 131072-sample render.
    //
    // The floor never reaches zero, and that is the circuit rather than the model: V1a sits ahead
    // of the pot and its grid conducts at 0.203 V, so a 0.2 input clips it wherever the knob is.
    // What the knob has to do is put V2a, V3a and V4a past their own knees on top of that. On
    // 2026-09-06 this test read 4.43 %, 3.73 % and 4.20 % with kV1aToV2aLevelStaging at 1/8, which
    // is V1a's floor and nothing else; at 5/8 the offline chain reads 4.3 %, 17.0 % and 24.2 %.
    constexpr double sample_rate = 48000.0;
    const double frequency = SushiDSP::measure::bin_exact_frequency(220.0, sample_rate, SushiDSP::measure::kFftSize);

    auto thd_percent_for = [&](float gain) -> double
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(kUnityMasterDb);
        const std::vector<float> stimulus =
            SushiDSP::measure::sine_stimulus(frequency, sample_rate, 131072, 0.2f);
        const std::vector<float> rendered = SushiDSP::measure::render(node, stimulus, sample_rate, 256);
        return SushiDSP::measure::thd(rendered, frequency, sample_rate).thd_percent;
    };

    const double low = thd_percent_for(0.1f);
    const double middle = thd_percent_for(0.5f);
    const double high = thd_percent_for(1.0f);
    std::printf("[ measure ] THD %.3f%% at gain 0.1, %.3f%% at 0.5, %.3f%% at 1.0\n", low, middle, high);

    EXPECT_GT(middle, low);
    EXPECT_GT(high, middle);
    EXPECT_GT(high, 10.0);
}

TEST(Unit_Dsp, DiezelVh4NeverExceedsFullScaleWithoutALimiter)
{
    // A plain clamp is what bounds the output. Driving gain, master and output to their ceilings
    // must still leave every sample inside [-1, 1], and must actually reach the rail - otherwise
    // the test proves only that the amp has headroom.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_parameter("gain", 1.0f);
    node.set_parameter("master", 12.0f);
    node.set_parameter("output", 12.0f);

    std::vector<float> input(256), output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    bool clamped_at_least_once = false;
    for (int block = 0; block < 50; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float t = static_cast<float>(block * 256 + i) / 48000.0f;
            input[static_cast<std::size_t>(i)] = std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * t);
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
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

TEST(Unit_Dsp, DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // The shipped calibration point, and the test that pins DiezelVh4::kMasterUnityGain: a
    // -12 dBFS 1 kHz sine into an untouched amp comes back out at about -6 dBFS.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    float peak = 0.0f;
    for (int block = 0; block < 188; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample_index = static_cast<float>(block * 256 + i);
            input[static_cast<std::size_t>(i)] =
                0.251f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);

        // The first blocks are skipped so the measurement is of the settled amp rather than of the
        // filters' and the sag stage's startup transient.
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

TEST(Unit_Dsp, DiezelVh4DescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

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

TEST(Unit_Dsp, DiezelVh4CabinetStageEngagesAtANonDefaultBlockSize)
{
    // The cabinet's partition is derived from the sample rate, not from PrepareInfo::max_block, so
    // a host running any block size gets the cabinet.
    constexpr std::uint32_t block_size = 128;
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    auto run = [block_size](std::vector<float> cabinet) -> std::vector<float>
    {
        SushiDSP::DiezelVh4 node(std::move(cabinet));
        SushiDSP::PrepareInfo info{ 48000.0, block_size, 1 };
        node.prepare(info);
        node.set_gain(0.6f);
        node.set_master(0.0f);

        std::vector<float> input(block_size);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
        }
        std::vector<float> output(block_size, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };

        for (int block = 0; block < 10; ++block)
        {
            SushiDSP::ProcessContext context{ output_channels, input_channels, block_size, 0.0 };
            node.process(context);
        }
        return output;
    };

    const std::vector<float> without_cabinet = run({});
    const std::vector<float> with_cabinet = run(impulse_response);

    double total_difference = 0.0;
    for (std::size_t i = 0; i < without_cabinet.size(); ++i)
    {
        ASSERT_TRUE(std::isfinite(with_cabinet[i]));
        total_difference += std::abs(static_cast<double>(with_cabinet[i]) - without_cabinet[i]);
    }
    EXPECT_GT(total_difference, 0.0);
}

TEST(Unit_Dsp, DiezelVh4DryLatencyIsOneOversampledRegion)
{
    // V1a through the sag stage is a single 8x region, so the reported latency is one
    // Oversampler8x round trip. A host compensates by this number, so it is pinned against the
    // oversampler itself rather than against a literal.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_FALSE(node.has_cabinet());

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Unit_Dsp, DiezelVh4GainKnobMovesTheInputHighPassCorner)
{
    // C13's 1 nF into R14 and the gain pot is what voices channel 3: the knob moves the high-pass
    // corner ahead of the first distorting stage from 618 Hz at the bottom of its travel to 314 Hz
    // at the top (dossier section 6). What that buys at 300 Hz relative to 1 kHz is bounded
    // arithmetic, so the gate is stated rather than guessed: a first-order high-pass at 618 Hz
    // reads 300 Hz at -5.8 dB relative to 1 kHz, and at 314 Hz it reads -2.8 dB, so the whole
    // travel is worth 3.0 dB and the assertion below sits just inside it.
    //
    // The knob is measured at 0.02 rather than at 0, because the pot is a divider and a wiper at
    // the bottom grounds the channel: 618 Hz is the corner's limit as the knob approaches zero,
    // not a setting that passes signal.
    auto response_for = [](float gain)
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(kUnityMasterDb);
        return SushiDSP::measure::frequency_response(node, 48000.0, { 300.0, 1000.0 });
    };

    const auto lowest = response_for(0.02f);
    const auto highest = response_for(1.0f);

    const double low_knob_tilt = magnitude_at(lowest, 300.0) - magnitude_at(lowest, 1000.0);
    const double high_knob_tilt = magnitude_at(highest, 300.0) - magnitude_at(highest, 1000.0);
    std::printf("[ measure ] 300 Hz relative to 1 kHz: %.3f dB at gain 0.02, %.3f dB at gain 1.0\n",
                low_knob_tilt, high_knob_tilt);

    EXPECT_GT(high_knob_tilt - low_knob_tilt, 2.5);
}

TEST(Unit_Dsp, DiezelVh4DeepAtMaximumIsATenDecibelShelfAtEightyHertz)
{
    // Deep is a 10 dB low shelf at 80 Hz (dossier section 5: 80 Hz from the owner's manual, the
    // depth a modelling choice). A shelf carries half its decibels at its own corner, so the full
    // ten show up below it: the test asserts both, which is what separates "the shelf is there and
    // it is 10 dB deep" from "something down there moved".
    auto response_for = [](float deep)
    {
        SushiDSP::DiezelVh4 node;
        node.set_deep(deep);
        node.set_master(kUnityMasterDb);
        return SushiDSP::measure::frequency_response(node, 48000.0, { 20.0, 80.0 });
    };

    const auto flat = response_for(0.0f);
    const auto deepest = response_for(1.0f);

    const double lift_at_eighty = magnitude_at(deepest, 80.0) - magnitude_at(flat, 80.0);
    const double lift_at_twenty = magnitude_at(deepest, 20.0) - magnitude_at(flat, 20.0);
    std::printf("[ measure ] Deep lift %.3f dB at 80 Hz, %.3f dB at 20 Hz\n", lift_at_eighty, lift_at_twenty);

    EXPECT_NEAR(lift_at_eighty, 5.0, 1.5);
    EXPECT_NEAR(lift_at_twenty, 10.0, 1.5);
}

TEST(Unit_Dsp, DiezelVh4MixBusAttenuationIsApplied)
{
    // The mix bus costs this channel 23.6 dB before V4a ever sees it: R43 680 kOhm into the 48.1
    // kOhm the three grounded legs make, x0.0660 (dossier section 6), and a single-channel module
    // has to hard-code that shunt or the mixer triode is driven 24 dB too hard (section 8,
    // question 13).
    //
    // What is asserted is the node's own small-signal gain, not the dossier's cascade product:
    // section 6 says in terms not to test against a small-signal cascade figure, because at
    // 108.7 dB "nothing survives that linearly". The number below is the module's documented
    // chain - the dossier's stage gains and dividers together with the level-staging constants
    // DiezelVh4.cpp names and explains - measured at -60 dBFS, where every stage is still linear.
    //
    // It is arithmetic on one measurement, in the same sense as kMasterUnityGain. This test read
    // -0.560 dB on 2026-09-06, before the fix round moved kV1aToV2aLevelStaging from 1/8 to 5/8
    // (x5, +13.979 dB in a chain this quiet) and kMasterUnityGain from 0.71 to 2.148178
    // (+9.616 dB): -0.560 + 13.979 + 9.616 = 23.04. The next run reads it directly.
    constexpr double kExpectedSmallSignalGainDb = 23.04;

    SushiDSP::DiezelVh4 node;
    node.set_gain(0.5f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_volume(0.5f);
    node.set_deep(0.0f);
    node.set_presence(0.5f);
    node.set_master(kUnityMasterDb);

    constexpr double sample_rate = 48000.0;
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, sample_rate, SushiDSP::measure::kFftSize);
    constexpr float amplitude = 0.001f;   // -60 dBFS
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

    EXPECT_NEAR(gain_db, kExpectedSmallSignalGainDb, 1.0);
}
