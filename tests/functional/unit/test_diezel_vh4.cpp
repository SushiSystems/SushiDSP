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

TEST(Unit_Dsp, DiezelVh4ThdDoesNotFallAsGainRises)
{
    // The gain pot is this channel's only drive control, so it has to sweep the cascade from
    // nearly clean into saturation. Measured with measure::thd() at a bin-exact 220 Hz on the
    // settled tail of a 131072-sample render.
    //
    // The floor never reaches zero, and that is the circuit rather than the model: V1a sits ahead
    // of the pot and its grid conducts at 0.203 V, so a 0.2 input clips it wherever the knob is.
    // What the knob has to do is put V2a, V3a and V4a past their own knees on top of that.
    //
    // 2026-09-07, with the level staging replaced by the plate-swing ceiling and the phase
    // inverter's tail degeneration and power-grid scale in place: 37.867 %, 39.963 % and
    // 42.822 %. The floor is an order of magnitude higher than the 4.43 % this test read on
    // 2026-09-06 because the dividers now run at face value in volts, which leaves every stage
    // past V1a saturated at any knob position - the knob moves the reading by six points rather
    // than by twenty.
    //
    // What is asserted is therefore that the reading does not fall as the knob rises. Spec
    // section 4 allows 1.0 percentage points of fall; this test holds the tighter 0.5 the
    // readings support. Once every stage clips, THD saturates
    // as a metric, and a strictly rising spectrum is not a property the circuit has to have.
    // DiezelVh4HasACleanEndAtMinimumGain is what proves the knob's other end still works.
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

    constexpr double kFallTolerancePercentagePoints = 0.5;
    EXPECT_GT(middle, low - kFallTolerancePercentagePoints);
    EXPECT_GT(high, middle - kFallTolerancePercentagePoints);
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
    // -12 dBFS 82 Hz sine into an untouched amp comes back out at about -6 dBFS, the frequency
    // all four amplifiers pin at (DA5 spec section 6).
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
                0.251f * std::sin(2.0f * static_cast<float>(M_PI) * 82.0f * sample_index / 48000.0f);
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

TEST(Unit_Dsp, DiezelVh4HasACleanEndAtMinimumGain)
{
    // The acceptance the spec settled on after the level staging came out. Every stage past V1a
    // saturates at the default setting - dossier section 6 says the circuit is that hot, and the
    // grid probes in modules/diezel_vh4/README.md record where the grids sit - so what has to be
    // proved is the other end of the knob: turned down, into a small signal, the channel is still
    // an amplifier rather than a fuzz box.
    //
    // The knob is read at 0.01 rather than 0.0 because the GAIN pot is a plain divider to ground
    // and 0.0 is a short: the node is silent there and THD has no denominator. 0.01 is the first
    // position that passes signal. It read 0.0940 % on 2026-09-07 and 1.843 % on the volt-true
    // power stage the same day, the pair's class-AB handover now reaching the output.
    constexpr double sample_rate = 48000.0;
    const double frequency = SushiDSP::measure::bin_exact_frequency(220.0, sample_rate, SushiDSP::measure::kFftSize);

    SushiDSP::DiezelVh4 node;
    node.set_gain(0.01f);
    node.set_master(kUnityMasterDb);
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(frequency, sample_rate, 131072, 0.01f);   // -40 dBFS
    const std::vector<float> rendered = SushiDSP::measure::render(node, stimulus, sample_rate, 256);
    const double thd_percent = SushiDSP::measure::thd(rendered, frequency, sample_rate).thd_percent;

    std::printf("[ measure ] clean end THD %.4f%%\n", thd_percent);
    EXPECT_LT(thd_percent, 10.0);
}

TEST(Unit_Dsp, DiezelVh4GridProbesReportEveryStageInSignalOrder)
{
    // The probes are what modules/diezel_vh4/README.md's grid table is measured with, so they have
    // to cover the chain rather than a prefix of it: five stages, every one of them moving after a
    // render, and nothing but zero outside the range.
    SushiDSP::DiezelVh4 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    ASSERT_EQ(node.triode_stage_count(), std::size_t{ 5 });

    // Before any audio every stage rests at its bias point, so every probe reads zero.
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 40; ++block)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float sample_index = static_cast<float>(block * 256 + i);
            input[static_cast<std::size_t>(i)] =
                0.251f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
    EXPECT_EQ(node.grid_excursion_peak_volts(std::size_t{ 1000 }), 0.0f);

    // reset() clears the peaks with the rest of the state.
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
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
    // depth a modelling choice). A shelf carries half its decibels at its own corner, so 5 dB at
    // 80 Hz is what pins both the corner and the depth.
    //
    // Only 80 Hz is asserted. frequency_response() sweeps at -40 dBFS and the shelf sits after the
    // amp, so nothing here is clamped - a run with the master 20 dB down reads the same two
    // figures - but the swept-sine estimate stops isolating the shelf at 20 Hz. C13's input
    // high-pass - 314 Hz with the gain knob at its top, 618 Hz at its bottom, and above 314 at the
    // default position this test uses - has thrown the fundamental away before the first
    // distorting stage, so what reaches Deep down there is mostly the saturated chain's own
    // product rather than a 20 Hz tone, and
    // the lift reads 3.779 dB (2026-09-07) where the shelf itself is 10 dB. 80 Hz reads 4.622 dB
    // against a nominal 5, inside the half decibel this test allows.
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

    EXPECT_NEAR(lift_at_eighty, 5.0, 0.5);
}

TEST(Unit_Dsp, DiezelVh4MixBusAttenuationIsApplied)
{
    // The mix bus costs this channel 23.6 dB before V4a ever sees it: R43 680 kOhm into the 48.1
    // kOhm the three grounded legs make, x0.0660 (dossier section 6), and a single-channel module
    // has to hard-code that shunt or the mixer triode is driven 24 dB too hard (section 8,
    // question 13).
    //
    // The level is -120 dBFS because that is where this chain is still linear end to end: the
    // same measurement reads 70.853 dB at -80, 72.965 at -100 and 73.114 at -120. At -80 dBFS the
    // EL34 grids already carry 4.9 V, past the 4.1 V at which the cut-off tube's 35 V plate
    // headroom saturates, so the pair is compressing there by 2.3 dB. Dossier section 6 warns
    // against testing a cascade gain figure at all, because at 108.7 dB "nothing survives that
    // linearly" - which is true of the amp as it is played, and is why the level here is six
    // orders below it.
    //
    // Every factor at 1 kHz with the knobs at noon, each one a constant DiezelVh4.cpp names. A
    // TriodeStage's grid sits on Rg/(Rs + Rg) of its input, so each stage's own divider is on its
    // row with its gain:
    //
    //   V1a, grid divider 1M/(1M+10k), gain 56.8                x56.2376  -> 0.0056238 V
    //   C13 0.923278, the gain pot at 0.5 giving 0.326772       x0.301701 -> 0.0016967 V
    //   V2a's cathode shelf 0.879497, its Miller pole 0.999304  x0.878885 -> 0.0014912 V
    //   V2a, grid divider 3M3/(3M3+47k), gain 67.2              x66.2564  -> 0.0988016 V
    //   C18 0.997970, kV2aToV3aDivider 0.4996003                x0.498586 -> 0.0492611 V
    //   V3a's cathode shelf 0.969856, its Miller pole 0.929217  x0.901207 -> 0.0443944 V
    //   V3a, grid divider 500k/(500k+470k), gain 71.4           x36.8041  -> 1.6339 V
    //   the V3b follower 0.986, C20 1.000054                    x0.986053 -> 1.61111 V
    //   the FMV deck at noon, R40 folded into it                x0.388541 -> 0.625983 V
    //   volume 0.5 x 0.5, kMixBusAttenuation 0.0660             x0.0165   -> 0.0103287 V
    //   V4a, grid divider 48.1k/(48.1k+44.923k), gain 28.7      x14.8401  -> 0.153279 V
    //   the V4b follower 0.986, / kPhaseInverterDegeneration    x0.522032 -> 0.0800166 V
    //   the inverter, grid divider 1M/(1M+10k), gain 61.927402  x61.3143  -> 4.90620 V
    //                                                  product to the EL34 grids  x49062
    //
    // The EL34 pair differenced, kPowerTubeToTransformerAttenuation, the transformer and the sag
    // stage are the one block left unwritten, and they are worth x0.09225 here: 49062 x 0.09225 is
    // the 4526 the assertion pins, 73.114 dB on 2026-09-07, with the input at 1e-6 rather than the
    // 1e-4 the rows above are written for. The grid probes corroborate the table at that level -
    // it predicts 0.000229 V on V3a's grid against a measured 0.000244, and 0.000792 V on the
    // inverter's against 0.000853.
    //
    // kMasterUnityGain is absent on purpose: the knob is set to kUnityMasterDb, which is its
    // reciprocal, so the two cancel and the master contributes x1.
    constexpr double kExpectedSmallSignalGainDb = 73.114;

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
    constexpr float amplitude = 0.000001f;   // -120 dBFS
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

    EXPECT_NEAR(gain_db, kExpectedSmallSignalGainDb, 0.5);
}
