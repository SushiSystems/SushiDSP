/**************************************************************************/
/* test_peavey5150.cpp                                                    */
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
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/io/Ir.hpp"
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

    // measure::frequency_response() sweeps at -40 dBFS, and at the default setting that squares
    // every stage from V2A on, so a shelf measured there reads the amplifier's compression rather
    // than the shelf: 6.74 dB for presence and 5.75 dB for resonance on 2026-09-07's chain. Both
    // shelves are linear sections after the decimator, so the honest place to read them is with the
    // path ahead of them small-signal. Turning PRE GAIN and the input trim down is not enough on
    // its own - the trim floors at -24 dB and the power section is still saturating - so POST GAIN
    // comes down too: it sits ahead of the driver, the phase inverter and the output tubes, which
    // is where the remaining compression is. At this setting the two shelves read 9.90 dB and
    // 9.90 dB against their 10 dB target.
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

    // measure::thd() windows a 65536-point FFT at the end of the signal, so anything handed to it
    // must be at least that long or its offset underflows: 300 blocks of 256 frames is 76 800
    // samples.
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

    // The index order is the contract a host and a saved preset address these by, so it is pinned
    // here rather than merely checked for membership.
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
        node.set_parameter(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();
    for (std::uint16_t i = 0; i < kParameterCount; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter(static_cast<std::uint16_t>(99), 1.0f);
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
    node.set_parameter("pre_gain", 0.2f);
    node.set_parameter("low", 0.4f);
    node.set_parameter("mid", 0.5f);
    node.set_parameter("high", 0.6f);
    node.set_parameter("post_gain", -3.0f);
    node.set_parameter("resonance", 0.3f);
    node.set_parameter("presence", 0.7f);
    node.set_parameter("input_trim", 6.0f);
    node.set_parameter("output", -3.0f);

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
    node.set_parameter("does_not_exist", 1.0f);
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

    // The control-thread call stores a target and nothing else: the coefficients the audio thread
    // last built are still the old ones, which is exactly what "dirty" reports.
    node->set_parameter("low", 1.0f);
    EXPECT_TRUE(node->tone_stack_is_dirty());

    for (int block = 0; block < 40 && node->tone_stack_is_dirty(); ++block)
    {
        node->process(context);
    }
    EXPECT_FALSE(node->tone_stack_is_dirty());
}

TEST(Unit_Dsp, Peavey5150IsDcStableAcrossManyBlocksAtHighGain)
{
    // Every knob at both extremes, 200 blocks of a DC-offset drive each: the output must stay
    // finite throughout and the settled DC term must stay below -40 dBFS. A cascade of eight
    // grid-conduction solvers around a feedback-derived resonance shelf is where a stuck operating
    // point would show up, and it would show up as a drifting offset rather than as a NaN.
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
    // Spec section 4: once every stage clips, THD saturates, so the metric is not asked to rise -
    // it is asked not to fall. The tolerance is 1.0 percentage point across the knob's travel,
    // because the metric eases once the phase inverter clips and the stages ahead of it square the
    // wave into faster-falling harmonics.
    //
    // The sweep starts at 0.02, the first position that passes signal: VR2 at the bottom mutes the
    // amplifier and the reading there is the FFT's floor on denormals, not distortion.
    //
    // Measured 2026-09-07: 26.66, 26.97, 27.09, 27.02, 27.20, 27.66, 28.07, 28.02, 27.59 percent.
    // The sweep rises to 0.50 and then eases; the largest fall is 0.433 pp from 0.70 to 1.00.
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
    // What this proves is its name and no more: the amplifier's even-harmonic content is higher
    // at full pre gain than at 0.2. It was written to assert V2A's asymmetry - the 39 kOhm
    // unbypassed cathode starves that stage to 0.102 mA and puts its plate within 15 V of the rail
    // (dossier section 3), so it clips one half-cycle far harder than the other - but an
    // output-side measure cannot attribute even harmonics to one stage, and the sweep below is not
    // monotonic, so the test is named for what it measures.
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

    const double quiet = even_harmonic_ratio(0.2f);
    const double driven = even_harmonic_ratio(1.0f);
    std::printf("[ measure ] even harmonics %.5f at pre gain 0.2, %.5f at 1.0\n", quiet, driven);


    // Measured 2026-09-07: 0.00865 at pre gain 0.2 against 0.09843 at 1.0, a factor of 11.4. The
    // gate is a factor of 4.
    //
    // The measure is not monotonic across the knob - 0.0095, 0.0385, 0.0086, 0.1338, 0.0984 at pre
    // gain 0.05, 0.10, 0.20, 0.50 and 1.00 - because at this stimulus every stage from V5A on is
    // already on its bound, so the amplifier's even harmonics are the push-pull pair's as much as
    // V2A's. V2A's own grid does move from 2.81 V to 4.26 V across the same span, past the 4.00 V
    // at which it conducts, so V2A's asymmetry is real; what an output-side measure cannot do is
    // attribute it, which is why this test is named for the two points it compares.
    // modules/peavey5150/README.md, "The cold clipper's asymmetry", carries the sweep.
    EXPECT_GT(driven, quiet * 4.0);
}

TEST(Unit_Dsp, Peavey5150GridProbesReportEveryStageInSignalOrder)
{
    // The two test hooks the plate-swing spec added, shaped the way
    // Jcm800GridProbesReportEveryStageInSignalOrder is: spec section 4 asks for the readings to be
    // recorded rather than asserted, so this pins the hooks and not the figures. Eight stages in
    // signal order - V1A, V1B, V2A, V2B, V5B, V5A, V3A and the phase inverter.
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
    // Dossier section 6, quoting Peavey's own specification table: "Active Presence: +10 dB 2 kHz".
    // The shelf's corner sits at 600 Hz so its plateau has arrived by 2 kHz; the comment beside
    // kPresenceHz in Peavey5150.cpp carries why. Read on the small-signal path for the reason
    // above small_signal_node(); measured 2026-09-07: 3.32 dB flat, 13.22 dB full, a 9.90 dB lift.
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
    // Dossier section 6: "Active Resonance: +10 dB at the cabinet resonant frequency". The cabinet
    // resonance is a modelling choice at 100 Hz, the dossier derives none, and the shelf's own
    // corner sits at 330 Hz so its plateau has arrived by then. The knob is flat at 0 and full at
    // 1, so those two are the pair compared. Read on the small-signal path for the reason above
    // small_signal_node(); measured 2026-09-07: -7.83 dB flat, 2.07 dB full, a 9.90 dB lift.
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
    // A plain clamp is what bounds the output, so driving pre gain, post gain and output to their
    // ceilings must still leave every sample inside [-1, 1] - and must actually reach the rail, or
    // the test would be proving only that the amp has headroom.
    auto node = prepared_node();
    node->set_parameter("pre_gain", 1.0f);
    node->set_parameter("post_gain", 12.0f);
    node->set_parameter("output", 12.0f);

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
    // The shipped calibration point, and the measurement that pins kPostGainUnityGain: a -12 dBFS
    // 1 kHz sine into an untouched amp comes back out at about -6 dBFS, leaving headroom for a
    // hotter pickup without the post-cabinet clamp engaging on a normal signal.
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
                0.251f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * sample_index / 48000.0f);
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node->process(context);

        // The first blocks are skipped so the measurement is of the settled amp, not of the
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
    std::printf("[ measure ] default peak %.3f dBFS; kPostGainUnityGain would need x%.6f\n",
                static_cast<double>(peak_decibels),
                static_cast<double>(0.5011872f / std::max(peak, 1e-9f)));
    EXPECT_GT(peak_decibels, -7.0f);
    EXPECT_LT(peak_decibels, -5.0f);
}

TEST(Unit_Dsp, Peavey5150HasACleanEndAtMinimumGain)
{
    // Spec section 4's acceptance for the plate-swing phase: a player who turns PRE GAIN down must
    // reach a setting the amplifier does not square. The 5150 asserts that at Peavey's own nominal
    // lead input and no louder: -80 dBV is 1e-4 V RMS, so 1.4142e-4 V peak, which against
    // kFullScaleVolts 2.0 is an amplitude of 7.071e-5 full scale, -83.01 dBFS. A 20 mV guitar sits
    // 46 dB above the nominal the manufacturer prints for this channel, so the -40 dBFS figure is
    // measured and printed but not asserted.
    //
    // The knob is read at 0.02, the lowest position this test uses. pre_gain_attenuation() is
    // n * sqrt(n), exactly zero only at the bottom of VR2's travel, so 0.0 mutes the amplifier and
    // the THD there is the FFT's own floor on denormals rather than distortion. Positions between
    // pass signal - 0.01 gives 0.001 - so 0.02 is a choice, not a threshold.
    //
    // Measured 2026-09-07, once the pair's degeneration was corrected to 1.905 and its stage gain
    // to the full 56.5: 1.340 % at the nominal input, and 25.652 % at -40 dBFS. At -40 dBFS the
    // amplifier squares from V5A on - V5A, V3A and the pair all sit on the solver's 4.136 V fence,
    // with V5B at 2.71 V behind them - so V5A is the stage that binds there.
    // modules/peavey5150/README.md, "The clean end", carries the table.
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

TEST(Unit_Dsp, Peavey5150CabinetStageEngagesAtANonDefaultBlockSize)
{
    // The cabinet's partition is derived from the sample rate, not from PrepareInfo::max_block, so
    // a host running any block size gets the cabinet, not only a 256-frame one.
    constexpr std::uint32_t block_size = 128;
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    auto run = [block_size](std::vector<float> cabinet) -> std::vector<float>
    {
        SushiDSP::Peavey5150 node(std::move(cabinet));
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, block_size, 1 });

        std::vector<float> input(block_size), output(block_size, 0.0f);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * static_cast<float>(i) / 48000.0f);
        }
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
        total_difference += std::fabs(static_cast<double>(with_cabinet[i]) - without_cabinet[i]);
    }
    EXPECT_GT(total_difference, 0.0);
}

TEST(Unit_Dsp, Peavey5150RejectsACabinetImpulseResponseAtTheWrongSampleRate)
{
    // StreamingConvolution convolves in samples, not seconds, so an IR captured at another rate
    // would play back at the wrong length and the wrong frequency response.
    auto node = prepared_node();
    EXPECT_FALSE(node->has_cabinet());
    const std::uint32_t dry_latency = node->latency_samples();

    SushiDSP::Ir wrong_rate;
    wrong_rate.sample_rate = 44100.0;
    wrong_rate.samples.assign(512, 0.0f);
    wrong_rate.samples[0] = 1.0f;
    EXPECT_FALSE(node->set_cabinet(wrong_rate));
    EXPECT_FALSE(node->has_cabinet());
    EXPECT_EQ(node->latency_samples(), dry_latency);

    SushiDSP::Ir right_rate = wrong_rate;
    right_rate.sample_rate = 48000.0;
    EXPECT_TRUE(node->set_cabinet(right_rate));
    EXPECT_TRUE(node->has_cabinet());
    node->reclaim_cabinets();
}

TEST(Unit_Dsp, Peavey5150DryLatencyIsOneOversampledRegion)
{
    // The whole amplifier is one 8x region, so the reported dry latency is one Oversampler8x round
    // trip. A host compensates by this number, so it is pinned against the oversampler itself.
    auto node = prepared_node();
    EXPECT_FALSE(node->has_cabinet());

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node->latency_samples(), reference.latency_samples());
}
