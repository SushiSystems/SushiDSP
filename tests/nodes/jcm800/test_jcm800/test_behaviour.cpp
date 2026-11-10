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
 * @brief Tests the Jcm800 node's parameters, levels, distortion and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>
#include "support/tolerance.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Samples the settled-output tests skip before they measure: the sag stage's and the
    /// filters' startup transient is over inside 21 blocks (5376 samples); this is three times
    /// that, rounded to a power of two. Characterized.
    constexpr std::size_t K_SETTLE_SAMPLES = 16384u;

    /// @brief The residual DC the settled output may carry, in full-scale units (-40 dBFS).
    /// Characterized.
    constexpr double K_RESIDUAL_DC_BOUND = 0.01;

    /// @brief The peak the settled output may reach with the master at its minimum, in
    /// full-scale units. Characterized: the first version of the test used this figure.
    constexpr double K_MASTER_MINIMUM_PEAK_BOUND = 0.01;

    /// @brief How far the output level at a higher preamp may sit below the one at a lower
    /// preamp, as a ratio. Characterized: the knob moves the level by +5.3 % (README), and
    /// 1 % is the sag stage's drift over the run.
    constexpr double K_SAG_DRIFT_RATIO = 0.99;

    /// @brief The factor a loud note must exceed a quiet one by at the bottom of the preamp
    /// knob. Characterized: the reading is 16.2, and a clamped square wave would read 1.
    constexpr double K_LEVEL_TRACKING_RATIO_FLOOR = 2.0;

    /// @brief How far THD may fall while the preamp rises, in percentage points. Characterized:
    /// the spec's second acceptance criterion allows 1.0.
    constexpr double K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS = 1.0;

    /// @brief THD the top of the preamp knob must exceed, in percent. Characterized: the
    /// reading on 2026-09-07 is 44.986 %.
    constexpr double K_SATURATED_THD_FLOOR_PERCENT = 20.0;

    /// @brief THD the bottom of the preamp knob must stay under, in percent. Characterized.
    constexpr double K_CLEAN_END_THD_CEILING_PERCENT = 10.0;

    /// @brief The level a -12 dBFS 82 Hz sine reaches at default settings, in dBFS.
    constexpr double K_CALIBRATION_PEAK_DBFS = -6.0;

    /// @brief How far the calibration peak may sit from the figure above, in dB. Characterized:
    /// the first version of the test used a window of -8 to -4 dBFS.
    constexpr double K_CALIBRATION_BAND_DB = 2.0;

    /// @brief How far the second harmonic with the power tubes' bias mismatch must sit below
    /// the one without it, in dB. Characterized: the reading is -0.167 dB (README), and the
    /// gate is an order of magnitude under it.
    constexpr double K_MISMATCH_SECOND_HARMONIC_SHIFT_DB = 0.01;

    /// @brief How far the settled peak after a tone change may sit from a fresh node's, in dB.
    /// Characterized: the sag stage's state differs between the two runs.
    constexpr double K_TONE_CHANGE_BAND_DB = 0.5;

    /// @brief Float roundings the level ratio of a linear gain may collect: the decibel
    /// conversion, the product and the mean square. Characterized.
    constexpr double K_LINEAR_GAIN_ROUNDINGS = 16.0;

    // unity_master_db is derived from K_MASTER_UNITY_GAIN. See tests/nodes/jcm800/test_jcm800/README.md.
    const float unity_master_db =
        -SushiDSP::gain_to_decibels(SushiDSP::Jcm800::K_MASTER_UNITY_GAIN);

    /// @brief A prepared node at the shared block size.
    void prepare_node(SushiDSP::Jcm800& node)
    {
        node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    }

    /// @brief Streams a sine through a prepared node block by block and returns every output sample.
    /// @param node The node, prepared at K_BLOCK.
    /// @param frequency_hz Frequency of the sine.
    /// @param amplitude Peak of the sine in full-scale units.
    /// @param blocks Number of K_BLOCK-frame blocks.
    std::vector<float> stream_sine(SushiDSP::Jcm800& node, double frequency_hz, double amplitude,
                                   std::size_t blocks)
    {
        std::vector<float> input(K_BLOCK, 0.0f);
        std::vector<float> output(blocks * K_BLOCK, 0.0f);
        for (std::size_t block = 0; block < blocks; ++block)
        {
            for (std::size_t i = 0; i < K_BLOCK; ++i)
            {
                const double n = static_cast<double>(block * K_BLOCK + i);
                input[i] = static_cast<float>(amplitude *
                                              std::sin(2.0 * M_PI * frequency_hz * n / K_SAMPLE_RATE));
            }
            const float* input_channels[1] = {input.data()};
            float* output_channels[1] = {output.data() + block * K_BLOCK};
            SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};
            node.process(context);
        }
        return output;
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

    /// @brief Root mean square from `begin` to the end.
    double rms_from(const std::vector<float>& samples, std::size_t begin)
    {
        double sum = 0.0;
        for (std::size_t n = begin; n < samples.size(); ++n)
        {
            sum += static_cast<double>(samples[n]) * samples[n];
        }
        return std::sqrt(sum / static_cast<double>(samples.size() - begin));
    }

    /// @brief Sets a preamp, the master at 0 dB and every tone knob at noon.
    void set_noon_tone(SushiDSP::Jcm800& node, float preamp)
    {
        node.set_preamp(preamp);
        node.set_master(0.0f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);
    }

    /// @brief The settled RMS of a 220 Hz sine through a fresh node at a preamp, after 15 blocks.
    double settled_rms_at_preamp(float preamp)
    {
        SushiDSP::Jcm800 node;
        set_noon_tone(node, preamp);
        prepare_node(node);
        return rms_from(stream_sine(node, 220.0, 0.05, 30), 15 * K_BLOCK);
    }

    /// @brief The settled peak of a 220 Hz sine through a fresh node at preamp 0.02, after 15 blocks.
    float settled_peak_at_input(double amplitude)
    {
        SushiDSP::Jcm800 node;
        set_noon_tone(node, 0.02f);
        prepare_node(node);
        return peak_from(stream_sine(node, 220.0, amplitude, 30), 15 * K_BLOCK);
    }

    /// @brief The settled RMS of a 220 Hz sine at preamp 0.6 through a node with trim and output set.
    double settled_rms_with_levels(float input_trim_db, float output_db)
    {
        SushiDSP::Jcm800 node;
        node.set_preamp(0.6f);
        node.set_master(0.0f);
        node.set_input_trim(input_trim_db);
        node.set_output(output_db);
        prepare_node(node);
        return rms_from(stream_sine(node, 220.0, 0.02, 30), 20 * K_BLOCK);
    }

    /// @brief THD in percent of a bin-exact sine through a fresh node at a preamp, after the startup transient.
    double thd_percent_at(float preamp, float amplitude, double frequency_hz)
    {
        SushiDSP::Jcm800 node;
        node.set_preamp(preamp);
        node.set_master(0.0f);
        const double frequency = SushiDSP::measure::bin_exact_frequency(
            frequency_hz, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        const std::vector<float> rendered = SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                             K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE, amplitude),
            K_SAMPLE_RATE, K_BLOCK);
        return SushiDSP::measure::thd(rendered, frequency, K_SAMPLE_RATE).thd_percent;
    }

    /// @brief The second harmonic, in dB below the fundamental, of a 220 Hz sine through a node.
    double second_harmonic_db(SushiDSP::Jcm800& node)
    {
        const double frequency = SushiDSP::measure::bin_exact_frequency(
            220.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        const std::vector<float> rendered = SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                             K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE, 0.2f),
            K_SAMPLE_RATE, K_BLOCK);
        return SushiDSP::measure::thd(rendered, frequency, K_SAMPLE_RATE).per_harmonic_db[1];
    }

    /// @brief Sets the node to the second-harmonic test's knob positions.
    void set_mismatch_test_knobs(SushiDSP::Jcm800& node)
    {
        node.set_preamp(0.5f);
        node.set_master(unity_master_db);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);
    }

    /// @brief Names a parameter: its id and a value to set and read back, in descriptor order.
    struct ParameterRow
    {
            const char* id;
            float value;
    };

    /// @brief The eight parameters in the order hosts and presets address them by.
    const ParameterRow K_PARAMETERS[8] = {{"preamp", 0.2f},   {"master", -6.0f},     {"bass", 0.4f},
                                          {"middle", 0.45f},  {"treble", 0.6f},      {"presence", 0.7f},
                                          {"input_trim", 12.0f}, {"output", -3.0f}};

    /// @brief Names a decibel parameter: its descriptor index and range.
    struct DecibelRow
    {
            std::size_t index;
            float min;
            float max;
            float default_value;
    };

    const DecibelRow K_DECIBEL_ROWS[3] = {{1, -60.0f, 12.0f, 0.0f}, {6, -24.0f, 24.0f, 0.0f}, {7, -60.0f, 12.0f, 0.0f}};

    /// @brief Names one drive position for the full-scale rows: tone knobs and the output level.
    struct DriveRow
    {
            const char* name;
            float tone;
            float output_db;
    };

    const DriveRow K_DRIVE_ROWS[2] = {{"AllKnobsUp", 1.0f, 0.0f}, {"OutputUp", 0.5f, 12.0f}};

    /// @brief Sets preamp and master to their ceilings, then the row's tone knobs and output.
    void set_drive(SushiDSP::Jcm800& node, const DriveRow& row)
    {
        node.set_preamp(1.0f);
        node.set_master(12.0f);
        node.set_bass(row.tone);
        node.set_middle(row.tone);
        node.set_treble(row.tone);
        node.set_presence(row.tone);
        node.set_output(row.output_db);
    }

    /// @brief Names a pair of preamp settings the THD must not fall between.
    struct PreampStep
    {
            const char* name;
            float lower;
            float upper;
    };

    const PreampStep K_PREAMP_STEPS[10] = {
        {"From0p02To0p1", 0.02f, 0.1f}, {"From0p1To0p2", 0.1f, 0.2f}, {"From0p2To0p3", 0.2f, 0.3f},
        {"From0p3To0p4", 0.3f, 0.4f},   {"From0p4To0p5", 0.4f, 0.5f}, {"From0p5To0p6", 0.5f, 0.6f},
        {"From0p6To0p7", 0.6f, 0.7f},   {"From0p7To0p8", 0.7f, 0.8f}, {"From0p8To0p9", 0.8f, 0.9f},
        {"From0p9To1", 0.9f, 1.0f}};
} // namespace

TEST(Medium_Jcm800, Jcm800ProducesOnlyFiniteOutputOverManyBlocks)
{
    SushiDSP::Jcm800 node;
    set_noon_tone(node, 0.6f);
    prepare_node(node);
    expect_all_finite(stream_sine(node, 220.0, 0.2, 20));
}

class Medium_Jcm800FullScale : public ::testing::TestWithParam<DriveRow>
{
};

TEST_P(Medium_Jcm800FullScale, OutputStaysInsideFullScale)
{
    // The clamp bounds the output at the rail. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    set_drive(node, GetParam());
    prepare_node(node);

    EXPECT_LE(peak_from(stream_sine(node, 220.0, 1.0, 40), 0), 1.0f);
}

INSTANTIATE_TEST_SUITE_P(Drives, Medium_Jcm800FullScale, ::testing::ValuesIn(K_DRIVE_ROWS),
                         [](const ::testing::TestParamInfo<DriveRow>& info)
                         { return std::string(info.param.name); });

TEST(Medium_Jcm800, Jcm800OutputReachesTheRailAtMaximumDrive)
{
    // If nothing ever reaches 1.0 the bound above proves only headroom. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    set_drive(node, K_DRIVE_ROWS[1]);
    prepare_node(node);

    EXPECT_EQ(peak_from(stream_sine(node, 220.0, 1.0, 50), 0), 1.0f);
}

TEST(Medium_Jcm800, Jcm800MasterAtMinimumProducesSilenceAfterSettling)
{
    SushiDSP::Jcm800 node;
    set_noon_tone(node, 0.6f);
    node.set_master(-60.0f);
    prepare_node(node);

    EXPECT_LT(peak_from(stream_sine(node, 220.0, 0.3, 20), 15 * K_BLOCK), K_MASTER_MINIMUM_PEAK_BOUND);
}

TEST(Medium_Jcm800, Jcm800PreampIncreasesOutputEnergy)
{
    // The preamp knob never costs level; 1 % is sag drift. See tests/nodes/jcm800/test_jcm800/README.md.
    EXPECT_GE(settled_rms_at_preamp(1.0f), settled_rms_at_preamp(0.1f) * K_SAG_DRIFT_RATIO);
}

TEST(Medium_Jcm800, Jcm800OutputTracksInputLevelRatherThanPinningToTheSolverClamp)
{
    // The knob is read at 0.02; at zero there is no output. See tests/nodes/jcm800/test_jcm800/README.md.
    EXPECT_GT(settled_peak_at_input(0.251), settled_peak_at_input(0.01) * K_LEVEL_TRACKING_RATIO_FLOOR);
}

TEST(Small_Jcm800, Jcm800DescriptorListsTheParametersInContractOrder)
{
    // The index order is the contract of hosts and presets. See tests/nodes/jcm800/test_jcm800/README.md.
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
    ASSERT_EQ(descriptor.parameter_count, 8u);
    for (std::size_t i = 0; i < 8; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, K_PARAMETERS[i].id) << i;
    }
}

TEST(Small_Jcm800, Jcm800DescriptorGivesTheLevelParametersDecibelKindAndRange)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
    for (const DecibelRow& row : K_DECIBEL_ROWS)
    {
        EXPECT_EQ(descriptor.parameters[row.index].kind, SushiDSP::ParamKind::Decibel) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].min, row.min) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].max, row.max) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].default_value, row.default_value) << row.index;
    }
}

TEST(Medium_Jcm800, Jcm800ToneChangeReachesTheSameSettledLevelAsAFreshNode)
{
    // The control thread stores a target and the audio thread walks to it. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 fresh;
    set_noon_tone(fresh, 0.6f);
    fresh.set_bass(1.0f);
    prepare_node(fresh);

    SushiDSP::Jcm800 changed;
    set_noon_tone(changed, 0.6f);
    prepare_node(changed);
    stream_sine(changed, 220.0, 0.2, 40);
    changed.set_parameter_for_control_thread("bass", 1.0f);

    const std::size_t settled = 160 * K_BLOCK;
    const double fresh_peak = peak_from(stream_sine(fresh, 220.0, 0.2, 200), settled);
    const double changed_peak = peak_from(stream_sine(changed, 220.0, 0.2, 160), settled - 40 * K_BLOCK);
    EXPECT_NEAR(20.0 * std::log10(changed_peak / fresh_peak), 0.0, K_TONE_CHANGE_BAND_DB);
}

TEST(Small_Jcm800, Jcm800IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::Jcm800 node;
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        node.set_parameter_for_audio_thread(i, K_PARAMETERS[i].value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_Jcm800, Jcm800IndexOutOfRangeChangesNoParameter)
{
    SushiDSP::Jcm800 node;
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

TEST(Small_Jcm800, Jcm800GenericParameterRoundTrip)
{
    SushiDSP::Jcm800 node;
    for (const ParameterRow& row : K_PARAMETERS)
    {
        node.set_parameter_for_control_thread(row.id, row.value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_Jcm800, Jcm800DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::Jcm800 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_Jcm800, Jcm800UnknownParameterIsSafeNoOp)
{
    SushiDSP::Jcm800 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Medium_Jcm800, Jcm800OutputControlScalesTheSignalByItsDecibels)
{
    // The output control is linear and post-cabinet. See tests/nodes/jcm800/test_jcm800/README.md.
    const double halving_db = 20.0 * std::log10(0.5);
    const double reference = settled_rms_with_levels(0.0f, 0.0f);
    const double halved = settled_rms_with_levels(0.0f, static_cast<float>(halving_db));
    EXPECT_NEAR(halved, reference * 0.5,
                reference * K_LINEAR_GAIN_ROUNDINGS * TestSupport::unit_roundoff<float>());
}

TEST(Medium_Jcm800, Jcm800InputTrimDrivesTheChainHarder)
{
    // The trim sits ahead of the nonlinear chain. See tests/nodes/jcm800/test_jcm800/README.md.
    EXPECT_GT(settled_rms_with_levels(6.0f, 0.0f), settled_rms_with_levels(0.0f, 0.0f));
}

TEST(Medium_Jcm800, Jcm800IsDcStableAcrossManyBlocksAtHighPreamp)
{
    SushiDSP::Jcm800 node;
    node.set_preamp(1.0f);
    node.set_master(unity_master_db);
    node.set_presence(1.0f);
    prepare_node(node);

    const std::vector<float> output = stream_sine(node, 220.0, 0.2, 200);
    expect_all_finite(output);
    EXPECT_LT(std::abs(mean_from(output, 100 * K_BLOCK)), K_RESIDUAL_DC_BOUND);
}

class Medium_Jcm800Thd : public ::testing::TestWithParam<PreampStep>
{
};

TEST_P(Medium_Jcm800Thd, DoesNotFallAsPreampRises)
{
    // THD must not fall as the preamp knob rises. See tests/nodes/jcm800/test_jcm800/README.md.
    const PreampStep& step = GetParam();
    EXPECT_GT(thd_percent_at(step.upper, 0.2f, 220.0),
              thd_percent_at(step.lower, 0.2f, 220.0) - K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS);
}

INSTANTIATE_TEST_SUITE_P(Steps, Medium_Jcm800Thd, ::testing::ValuesIn(K_PREAMP_STEPS),
                         [](const ::testing::TestParamInfo<PreampStep>& info)
                         { return std::string(info.param.name); });

TEST(Medium_Jcm800, Jcm800ThdAtMaximumPreampIsSaturated)
{
    // Guards the sweep above against being satisfied by a run of near-zero readings.
    EXPECT_GT(thd_percent_at(1.0f, 0.2f, 220.0), K_SATURATED_THD_FLOOR_PERCENT);
}

TEST(Medium_Jcm800, Jcm800PowerTubeBiasMismatchLowersTheSecondHarmonic)
{
    // The shipped mismatch is compared with a zeroed one. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 shipped;
    set_mismatch_test_knobs(shipped);

    SushiDSP::Jcm800 zeroed;
    set_mismatch_test_knobs(zeroed);
    zeroed.set_power_tube_grid_bias_mismatch_volts(0.0f);

    EXPECT_LT(second_harmonic_db(shipped) - second_harmonic_db(zeroed), -K_MISMATCH_SECOND_HARMONIC_SHIFT_DB);
}

TEST(Medium_Jcm800, Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS low-E sine comes out at about -6 dBFS. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    prepare_node(node);
    // The first blocks hold the startup transient. See tests/nodes/jcm800/test_jcm800/README.md.
    const std::vector<float> output = stream_sine(node, 82.0, 0.251, 188);
    const double peak_decibels = 20.0 * std::log10(static_cast<double>(peak_from(output, 21 * K_BLOCK)));
    EXPECT_NEAR(peak_decibels, K_CALIBRATION_PEAK_DBFS, K_CALIBRATION_BAND_DB);
}

TEST(Small_Jcm800, Jcm800DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    prepare_node(node);

    SushiDSP::Oversampler8x reference(K_BLOCK);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Medium_Jcm800, Jcm800HasACleanEndAtMinimumGain)
{
    // The knob is read at 0.01 because 0.0 passes no signal. See tests/nodes/jcm800/test_jcm800/README.md.
    EXPECT_LT(thd_percent_at(0.01f, 0.01f, 1000.0), K_CLEAN_END_THD_CEILING_PERCENT); // -40 dBFS
}

TEST(Small_Jcm800, Jcm800GridProbesReadZeroAtRestForEveryStage)
{
    SushiDSP::Jcm800 node;
    prepare_node(node);
    ASSERT_EQ(node.triode_stage_count(), std::size_t{4});
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Medium_Jcm800, Jcm800GridProbesMoveForEveryStageAfterAudio)
{
    // Every stage is in the signal path, so a signal that reaches V1a reaches all four.
    SushiDSP::Jcm800 node;
    prepare_node(node);
    stream_sine(node, 220.0, 0.251, 40);
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Small_Jcm800, Jcm800GridProbeOutOfRangeReadsZero)
{
    SushiDSP::Jcm800 node;
    prepare_node(node);
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
    EXPECT_EQ(node.grid_excursion_peak_volts(std::size_t{99}), 0.0f);
}

TEST(Medium_Jcm800, Jcm800ResetClearsTheGridProbes)
{
    SushiDSP::Jcm800 node;
    prepare_node(node);
    stream_sine(node, 220.0, 0.251, 40);
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}
