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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_behaviour.cpp
 * @brief Tests the Peavey5150 node's parameters, levels, distortion and shelves.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/peavey5150/peavey5150.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;
    constexpr std::size_t K_PARAMETER_COUNT = 9;

    /// @brief Samples the settled-output tests skip before they measure: three times the 21-block
    /// startup transient of the filters and the sag stage, rounded to a power of two. Characterized.
    constexpr std::size_t K_SETTLE_SAMPLES = 16384u;

    /// @brief The settled DC the output may carry, in full-scale units (-40 dBFS).
    constexpr double K_RESIDUAL_DC_BOUND = 0.01;

    /// @brief How far THD may fall while Pre Gain rises, in percentage points (spec section 4).
    constexpr double K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS = 1.0;

    /// @brief THD the top of Pre Gain must exceed, in percent. Characterized: the reading on
    /// 2026-09-07 is 27.25 %.
    constexpr double K_SATURATED_THD_FLOOR_PERCENT = 3.0;

    /// @brief The factor by which the even harmonics at full Pre Gain must exceed those at a low
    /// drive. Characterized: the reading on 2026-09-07 is 3.18.
    constexpr double K_EVEN_HARMONIC_RISE_FACTOR = 2.0;

    /// @brief The Pre Gain whose drive equals the one the comparison was calibrated at: the
    /// exponential audio taper passes 0.089443 of the pot at ln(1 + 80 * 0.089443) / ln 81.
    constexpr float K_QUIET_PRE_GAIN = 0.4776f;

    /// @brief The lead input Peavey specifies, -80 dBV, in full-scale units (-83.01 dBFS).
    constexpr float K_NOMINAL_LEAD_INPUT = 7.071e-5f;

    /// @brief THD the nominal lead input may read at the bottom of Pre Gain, in percent.
    /// Characterized: the first version of the test used this figure.
    constexpr double K_CLEAN_END_THD_CEILING_PERCENT = 5.0;

    /// @brief The level a -12 dBFS 82 Hz sine reaches at default settings, in dBFS (DA5 spec
    /// section 6).
    constexpr double K_CALIBRATION_PEAK_DBFS = -6.0;

    /// @brief How far the calibration peak may sit from the figure above, in dB. Characterized:
    /// the first version of the test used a window of -7 to -5 dBFS.
    constexpr double K_CALIBRATION_BAND_DB = 1.0;

    /// @brief The lift Peavey's specification table gives Presence at 2 kHz and Resonance at
    /// 100 Hz, in dB (dossier section 6).
    constexpr double K_SHELF_LIFT_DB = 10.0;

    /// @brief How far a shelf's lift may sit from the figure above, in dB. Characterized: the
    /// readings are 9.90 dB (README).
    constexpr double K_SHELF_LIFT_BAND_DB = 1.0;

    /// @brief How far the settled peak after a tone change may sit from a fresh node's, in dB.
    /// Characterized: the sag stage's state differs between the two runs.
    constexpr double K_TONE_CHANGE_BAND_DB = 0.5;

    // The shelves are read with a small-signal path ahead. See tests/nodes/peavey5150/test_peavey5150/README.md.
    constexpr float K_LINEAR_PATH_PRE_GAIN = 0.02f;
    constexpr float K_LINEAR_PATH_INPUT_TRIM_DB = -24.0f;
    constexpr float K_LINEAR_PATH_POST_GAIN_DB = -40.0f;

    /// @brief A node with every knob at its descriptor default, prepared at 48 kHz.
    std::unique_ptr<SushiDSP::Peavey5150> prepared_node()
    {
        auto node = std::make_unique<SushiDSP::Peavey5150>();
        node->prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
        return node;
    }

    /// @brief A default node set to a level at which the chain ahead of the shelves stays linear.
    std::unique_ptr<SushiDSP::Peavey5150> small_signal_node()
    {
        auto node = prepared_node();
        node->set_pre_gain(K_LINEAR_PATH_PRE_GAIN);
        node->set_input_trim(K_LINEAR_PATH_INPUT_TRIM_DB);
        node->set_post_gain(K_LINEAR_PATH_POST_GAIN_DB);
        return node;
    }

    /// @brief Prepares a node and renders a sine of `blocks` blocks through it.
    std::vector<float> render_sine(SushiDSP::Peavey5150& node, double frequency, float amplitude,
                                   std::size_t blocks)
    {
        return SushiDSP::measure::render(
            node, SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE, blocks * K_BLOCK, amplitude),
            K_SAMPLE_RATE, K_BLOCK);
    }

    /// @brief The whole node's harmonics at one pre-gain setting, for a bin-exact 220 Hz tone
    /// through the settled node (eight harmonics).
    /// @param pre_gain VR2's position.
    /// @param amplitude The stimulus amplitude.
    SushiDSP::measure::ThdResult harmonics_at(float pre_gain, float amplitude)
    {
        const double frequency = SushiDSP::measure::bin_exact_frequency(
            220.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        SushiDSP::Peavey5150 node;
        node.set_pre_gain(pre_gain);
        const std::vector<float> rendered = SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                             K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE, amplitude),
            K_SAMPLE_RATE, K_BLOCK);
        return SushiDSP::measure::thd(rendered, frequency, K_SAMPLE_RATE, 8);
    }

    /// @brief The root of the summed power of harmonics 2, 4, 6 and 8, against the fundamental.
    double even_harmonic_ratio(float pre_gain)
    {
        const SushiDSP::measure::ThdResult result = harmonics_at(pre_gain, 0.02f);
        double even = 0.0;
        for (std::size_t harmonic = 2; harmonic <= 8; harmonic += 2)
        {
            even += std::pow(10.0, result.per_harmonic_db[harmonic - 1] / 10.0);
        }
        return std::sqrt(even);
    }

    /// @brief The magnitude one frequency comes back at, in decibels.
    double response_at(SushiDSP::Peavey5150& node, double frequency)
    {
        return SushiDSP::measure::frequency_response(node, K_SAMPLE_RATE, {frequency}).front().magnitude_db;
    }

    /// @brief Fails for every sample that is not a finite number.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (std::size_t n = 0; n < samples.size(); ++n)
        {
            ASSERT_TRUE(std::isfinite(samples[n])) << n;
        }
    }

    /// @brief Largest magnitude from `begin` to the end.
    float peak_from(const std::vector<float>& samples, std::size_t begin)
    {
        float peak = 0.0f;
        for (std::size_t n = begin; n < samples.size(); ++n)
        {
            peak = std::max(peak, std::fabs(samples[n]));
        }
        return peak;
    }

    /// @brief Mean from `begin` to the end.
    double mean_from(const std::vector<float>& samples, std::size_t begin)
    {
        double sum = 0.0;
        for (std::size_t n = begin; n < samples.size(); ++n)
        {
            sum += samples[n];
        }
        return sum / static_cast<double>(samples.size() - begin);
    }

    /// @brief Names a parameter: its id and a value to set and read back, in descriptor order.
    struct ParameterRow
    {
            const char* id;
            float value;
    };

    /// @brief The nine parameters in the order hosts and presets address them by.
    const ParameterRow K_PARAMETERS[K_PARAMETER_COUNT] = {
        {"pre_gain", 0.2f},  {"low", 0.4f},        {"mid", 0.45f},        {"high", 0.6f},  {"post_gain", -6.0f},
        {"resonance", 0.3f}, {"presence", 0.7f},   {"input_trim", 12.0f}, {"output", -3.0f}};

    /// @brief Names a decibel parameter: its descriptor index and range.
    struct DecibelRow
    {
            std::size_t index;
            float min;
            float max;
            float default_value;
    };

    const DecibelRow K_DECIBEL_ROWS[3] = {{4, -60.0f, 12.0f, 0.0f}, {7, -24.0f, 24.0f, 0.0f}, {8, -60.0f, 12.0f, 0.0f}};

    /// @brief Names one stop of every knob for the settled-DC rows.
    struct Extreme
    {
            const char* name;
            float knob;
            float post_gain_db;
    };

    const Extreme K_EXTREMES[2] = {{"AllKnobsDown", 0.0f, -60.0f}, {"AllKnobsUp", 1.0f, 12.0f}};

    /// @brief Names a Pre Gain position the THD sweep reads.
    struct PreGainStep
    {
            const char* name;
            float lower;
            float upper;
    };

    const PreGainStep K_PRE_GAIN_STEPS[8] = {
        {"From0p02To0p05", 0.02f, 0.05f}, {"From0p05To0p1", 0.05f, 0.1f}, {"From0p1To0p2", 0.1f, 0.2f},
        {"From0p2To0p3", 0.2f, 0.3f},     {"From0p3To0p4", 0.3f, 0.4f},   {"From0p4To0p5", 0.4f, 0.5f},
        {"From0p5To0p7", 0.5f, 0.7f},     {"From0p7To1", 0.7f, 1.0f}};
} // namespace

TEST(Medium_Peavey5150, Peavey5150ProducesOnlyFiniteOutputOverManyBlocks)
{
    auto node = prepared_node();
    expect_all_finite(render_sine(*node, 220.0, 0.2f, 20));
}

TEST(Small_Peavey5150, Peavey5150DescriptorListsTheParametersInContractOrder)
{
    // The index order is the contract of hosts and presets. See tests/nodes/peavey5150/test_peavey5150/README.md.
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();
    ASSERT_EQ(descriptor.parameter_count, K_PARAMETER_COUNT);
    for (std::size_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, K_PARAMETERS[i].id) << i;
    }
}

TEST(Small_Peavey5150, Peavey5150DescriptorGivesTheLevelParametersDecibelKindAndRange)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::peavey5150_descriptor();
    for (const DecibelRow& row : K_DECIBEL_ROWS)
    {
        EXPECT_EQ(descriptor.parameters[row.index].kind, SushiDSP::ParamKind::Decibel) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].min, row.min) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].max, row.max) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].default_value, row.default_value) << row.index;
    }
}

TEST(Small_Peavey5150, Peavey5150IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::Peavey5150 node;
    for (std::uint16_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        node.set_parameter_for_audio_thread(i, K_PARAMETERS[i].value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_Peavey5150, Peavey5150IndexOutOfRangeChangesNoParameter)
{
    SushiDSP::Peavey5150 node;
    for (const ParameterRow& row : K_PARAMETERS)
    {
        node.set_parameter_for_control_thread(row.id, row.value);
    }
    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 1.0f);
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_Peavey5150, Peavey5150GenericParameterRoundTrip)
{
    SushiDSP::Peavey5150 node;
    for (const ParameterRow& row : K_PARAMETERS)
    {
        node.set_parameter_for_control_thread(row.id, row.value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_Peavey5150, Peavey5150DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::Peavey5150 node;
    node.set_post_gain(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_EQ(node.get_parameter("post_gain"), 12.0f);
    EXPECT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_Peavey5150, Peavey5150UnknownParameterIsSafeNoOp)
{
    SushiDSP::Peavey5150 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Medium_Peavey5150, Peavey5150ToneChangeReachesTheSameSettledLevelAsAFreshNode)
{
    // The control thread stores a target and the audio thread walks to it. See tests/nodes/peavey5150/test_peavey5150/README.md.
    SushiDSP::Peavey5150 fresh;
    fresh.set_low(1.0f);
    const std::size_t blocks = 200;
    const std::size_t settled = 160 * K_BLOCK;
    const double fresh_peak = peak_from(render_sine(fresh, 220.0, 0.2f, blocks), settled);

    SushiDSP::Peavey5150 changed;
    render_sine(changed, 220.0, 0.2f, 40);
    changed.set_parameter_for_control_thread("low", 1.0f);
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(220.0, K_SAMPLE_RATE, 160 * K_BLOCK, 0.2f);
    std::vector<float> output(stimulus.size(), 0.0f);
    for (std::size_t block = 0; block < 160; ++block)
    {
        const float* input_channels[1] = {stimulus.data() + block * K_BLOCK};
        float* output_channels[1] = {output.data() + block * K_BLOCK};
        SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};
        changed.process(context);
    }

    const double changed_peak = peak_from(output, 120 * K_BLOCK);
    EXPECT_NEAR(20.0 * std::log10(changed_peak / fresh_peak), 0.0, K_TONE_CHANGE_BAND_DB);
}

class Medium_Peavey5150Settled : public ::testing::TestWithParam<Extreme>
{
};

TEST_P(Medium_Peavey5150Settled, IsDcStableAcrossManyBlocksAtHighGain)
{
    // A stuck operating point shows as a drifting offset. See tests/nodes/peavey5150/test_peavey5150/README.md.
    const Extreme& row = GetParam();
    SushiDSP::Peavey5150 node;
    node.set_pre_gain(row.knob);
    node.set_low(row.knob);
    node.set_mid(row.knob);
    node.set_high(row.knob);
    node.set_resonance(row.knob);
    node.set_presence(row.knob);
    node.set_post_gain(row.post_gain_db);

    const std::vector<float> drive(200 * K_BLOCK, 0.1f);
    const std::vector<float> output = SushiDSP::measure::render(node, drive, K_SAMPLE_RATE, K_BLOCK);
    expect_all_finite(output);
    EXPECT_LT(std::fabs(mean_from(output, 150 * K_BLOCK)), K_RESIDUAL_DC_BOUND);
}

INSTANTIATE_TEST_SUITE_P(Extremes, Medium_Peavey5150Settled, ::testing::ValuesIn(K_EXTREMES),
                         [](const ::testing::TestParamInfo<Extreme>& info)
                         { return std::string(info.param.name); });

class Medium_Peavey5150Thd : public ::testing::TestWithParam<PreGainStep>
{
};

TEST_P(Medium_Peavey5150Thd, DoesNotFallAsPreGainRises)
{
    // The sweep starts at 0.02, where the knob passes signal. See tests/nodes/peavey5150/test_peavey5150/README.md.
    const PreGainStep& step = GetParam();
    EXPECT_GT(harmonics_at(step.upper, 0.02f).thd_percent,
              harmonics_at(step.lower, 0.02f).thd_percent - K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS);
}

INSTANTIATE_TEST_SUITE_P(Steps, Medium_Peavey5150Thd, ::testing::ValuesIn(K_PRE_GAIN_STEPS),
                         [](const ::testing::TestParamInfo<PreGainStep>& info)
                         { return std::string(info.param.name); });

TEST(Medium_Peavey5150, Peavey5150ThdAtMaximumPreGainIsSaturated)
{
    // Guards the sweep above against being satisfied by indistinguishable near-zero readings.
    EXPECT_GT(harmonics_at(1.0f, 0.02f).thd_percent, K_SATURATED_THD_FLOOR_PERCENT);
}

TEST(Medium_Peavey5150, Peavey5150EvenHarmonicsRiseFromLowToFullPreGain)
{
    // The two points are drive levels, not knob positions. See tests/nodes/peavey5150/test_peavey5150/README.md.
    const double quiet = even_harmonic_ratio(K_QUIET_PRE_GAIN);
    // The gate is a factor of 2 against a measured 3.18. See tests/nodes/peavey5150/test_peavey5150/README.md.
    EXPECT_GT(even_harmonic_ratio(1.0f), quiet * K_EVEN_HARMONIC_RISE_FACTOR);
}

TEST(Small_Peavey5150, Peavey5150ReportsEightTriodeStages)
{
    auto node = prepared_node();
    EXPECT_EQ(node->triode_stage_count(), std::size_t{8});
}

TEST(Small_Peavey5150, Peavey5150GridProbesReadZeroAtRestForEveryStage)
{
    auto node = prepared_node();
    for (std::size_t stage = 0; stage < node->triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node->grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Medium_Peavey5150, Peavey5150GridProbesMoveForEveryStageAfterAudio)
{
    SushiDSP::Peavey5150 node;
    render_sine(node, 1000.0, 0.251f, 40);
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Small_Peavey5150, Peavey5150GridProbeOutOfRangeReadsZero)
{
    // The probe hooks are pinned, not their figures. See tests/nodes/peavey5150/test_peavey5150/README.md.
    auto node = prepared_node();
    EXPECT_EQ(node->grid_excursion_peak_volts(node->triode_stage_count()), 0.0f);
    EXPECT_EQ(node->grid_excursion_peak_volts(std::size_t{99}), 0.0f);
}

TEST(Medium_Peavey5150, Peavey5150ResetClearsTheGridProbes)
{
    SushiDSP::Peavey5150 node;
    render_sine(node, 1000.0, 0.251f, 40);
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Medium_Peavey5150, Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels)
{
    // Presence at maximum lifts 2 kHz by 9.90 dB. See tests/nodes/peavey5150/test_peavey5150/README.md.
    auto flat = small_signal_node();
    flat->set_presence(0.5f);
    auto lifted = small_signal_node();
    lifted->set_presence(1.0f);

    EXPECT_NEAR(response_at(*lifted, 2000.0) - response_at(*flat, 2000.0), K_SHELF_LIFT_DB, K_SHELF_LIFT_BAND_DB);
}

TEST(Medium_Peavey5150, Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels)
{
    // Resonance at maximum lifts 100 Hz by 9.90 dB. See tests/nodes/peavey5150/test_peavey5150/README.md.
    auto flat = small_signal_node();
    flat->set_resonance(0.0f);
    auto lifted = small_signal_node();
    lifted->set_resonance(1.0f);

    EXPECT_NEAR(response_at(*lifted, 100.0) - response_at(*flat, 100.0), K_SHELF_LIFT_DB, K_SHELF_LIFT_BAND_DB);
}

TEST(Medium_Peavey5150, Peavey5150NeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/peavey5150/test_peavey5150/README.md.
    SushiDSP::Peavey5150 node;
    node.set_pre_gain(1.0f);
    node.set_post_gain(12.0f);
    node.set_output(12.0f);

    EXPECT_EQ(peak_from(render_sine(node, 220.0, 1.0f, 50), 0), 1.0f);
}

TEST(Medium_Peavey5150, Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/nodes/peavey5150/test_peavey5150/README.md.
    SushiDSP::Peavey5150 node;
    // The first blocks hold the startup transient. See tests/nodes/peavey5150/test_peavey5150/README.md.
    const std::vector<float> output = render_sine(node, 82.0, 0.251f, 188);
    const double peak_decibels = 20.0 * std::log10(static_cast<double>(peak_from(output, 21 * K_BLOCK)));
    EXPECT_NEAR(peak_decibels, K_CALIBRATION_PEAK_DBFS, K_CALIBRATION_BAND_DB);
}

TEST(Medium_Peavey5150, Peavey5150HasACleanEndAtMinimumGain)
{
    // The nominal lead input is -83.01 dBFS. See tests/nodes/peavey5150/test_peavey5150/README.md.
    EXPECT_LT(harmonics_at(0.02f, K_NOMINAL_LEAD_INPUT).thd_percent, K_CLEAN_END_THD_CEILING_PERCENT);
}

TEST(Small_Peavey5150, Peavey5150DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/peavey5150/test_peavey5150/README.md.
    auto node = prepared_node();

    SushiDSP::Oversampler8x reference(K_BLOCK);
    EXPECT_EQ(node->latency_samples(), reference.latency_samples());
}
