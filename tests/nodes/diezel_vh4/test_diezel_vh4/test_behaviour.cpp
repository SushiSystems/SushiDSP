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
 * @brief Tests the DiezelVh4 node's parameters, levels and frequency response.
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
#include <SushiDSP/nodes/diezel_vh4/diezel_vh4.hpp>
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

    /// @brief Samples the settled-output tests skip: the sag stage's and the filters' startup
    /// transient is over inside 21 blocks (5376 samples); this is three times that, rounded to a
    /// power of two. Characterized.
    constexpr std::size_t K_SETTLE_SAMPLES = 16384u;

    /// @brief The residual DC the settled output may carry, in full-scale units (-40 dBFS).
    /// Characterized.
    constexpr double K_RESIDUAL_DC_BOUND = 0.01;

    /// @brief How far THD may fall while the gain knob rises, in percentage points. Characterized:
    /// the readings support 0.5, and spec section 4 allows 1.0.
    constexpr double K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS = 0.5;

    /// @brief THD the top of the gain knob must exceed, in percent. Characterized: the readings
    /// on 2026-09-07 are 37.9 to 42.8 % across the knob.
    constexpr double K_SATURATED_THD_FLOOR_PERCENT = 10.0;

    /// @brief THD the bottom of the gain knob must stay under, in percent. Characterized: the
    /// reading on 2026-09-07 is 1.843 %.
    constexpr double K_CLEAN_END_THD_CEILING_PERCENT = 10.0;

    /// @brief The level a -12 dBFS 82 Hz sine reaches at default settings, in dBFS (DA5 spec
    /// section 6).
    constexpr double K_CALIBRATION_PEAK_DBFS = -6.0;

    /// @brief How far the calibration peak may sit from the spec's figure, in dB. Characterized:
    /// the spec says "about".
    constexpr double K_CALIBRATION_BAND_DB = 1.0;

    /// @brief The corners of the input high-pass at the bottom and the top of the gain knob, in
    /// hertz: C13 1 nF into R14 and the pot (dossier section 6).
    constexpr double K_HIGH_PASS_CORNER_LOW_KNOB_HZ = 618.0;
    constexpr double K_HIGH_PASS_CORNER_HIGH_KNOB_HZ = 314.0;

    /// @brief How far below the analytic travel the measured one may sit, in dB. Characterized:
    /// the knob is read at 0.02, not at the 618 Hz limit, and the rest of the chain adds a tilt.
    constexpr double K_TRAVEL_SLACK_DB = 0.5;

    /// @brief The depth of the Deep shelf in decibels (dossier section 5, a modelling choice).
    constexpr double K_DEEP_SHELF_DB = 10.0;

    /// @brief How far the lift at the shelf's corner may sit from half the depth, in dB.
    /// Characterized: the reading on 2026-09-07 is 4.813 dB.
    constexpr double K_SHELF_CORNER_BAND_DB = 0.5;

    /// @brief The small-signal gain of the whole chain at 1 kHz and -120 dBFS, in dB. Characterized:
    /// 49062 to the EL34 grids from the README table (93.81 dB) times 0.09225 of power stage,
    /// transformer and sag, which has no derivation; measured on 2026-09-07.
    constexpr double K_CHARACTERIZED_SMALL_SIGNAL_GAIN_DB = 73.114;

    /// @brief How far the small-signal gain may sit from the characterized one, in dB.
    constexpr double K_SMALL_SIGNAL_GAIN_BAND_DB = 0.5;

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

    /// @brief A prepared node at the shared block size.
    void prepare_node(SushiDSP::DiezelVh4& node)
    {
        node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    }

    /// @brief Streams a sine through a prepared node block by block and returns every output sample.
    /// @param node The node, prepared at K_BLOCK.
    /// @param frequency_hz Frequency of the sine.
    /// @param amplitude Peak of the sine in full-scale units.
    /// @param blocks Number of K_BLOCK-frame blocks.
    std::vector<float> stream_sine(SushiDSP::DiezelVh4& node, double frequency_hz, double amplitude,
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

    /// @brief THD in percent of a sine through a node at a gain, after the startup transient.
    double thd_percent_at(float gain, float amplitude)
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(unity_master_db);
        const double frequency = SushiDSP::measure::bin_exact_frequency(
            220.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        const std::vector<float> rendered = SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                             K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE, amplitude),
            K_SAMPLE_RATE, K_BLOCK);
        return SushiDSP::measure::thd(rendered, frequency, K_SAMPLE_RATE).thd_percent;
    }

    /// @brief The tilt, in dB, of a first-order high-pass at `corner_hz` between 300 Hz and 1 kHz.
    double high_pass_tilt_db(double corner_hz)
    {
        const auto magnitude = [corner_hz](double f)
        {
            const double ratio = f / corner_hz;
            return 20.0 * std::log10(ratio / std::sqrt(1.0 + ratio * ratio));
        };
        return magnitude(300.0) - magnitude(1000.0);
    }

    /// @brief Names a parameter: its id and a value to set and read back, in descriptor order.
    struct ParameterRow
    {
            const char* id;
            float value;
    };

    /// @brief The ten parameters in the order hosts and presets address them by.
    const ParameterRow K_PARAMETERS[10] = {{"gain", 0.2f},     {"bass", 0.3f},     {"middle", 0.4f},
                                           {"treble", 0.45f},  {"volume", 0.6f},   {"deep", 0.7f},
                                           {"presence", 0.8f}, {"master", -6.0f},  {"input_trim", 6.0f},
                                           {"output", -3.0f}};

    /// @brief Names a decibel parameter: its descriptor index and range.
    struct DecibelRow
    {
            std::size_t index;
            float min;
            float max;
            float default_value;
    };

    const DecibelRow K_DECIBEL_ROWS[3] = {{7, -60.0f, 12.0f, 0.0f}, {8, -24.0f, 24.0f, 0.0f}, {9, -60.0f, 12.0f, 0.0f}};

    /// @brief Names one knob extreme for the settled-output rows.
    struct KnobExtreme
    {
            std::string name;
            const char* knob;
            float value;
    };

    std::vector<KnobExtreme> knob_extremes()
    {
        std::vector<KnobExtreme> rows;
        for (const char* knob : {"gain", "bass", "middle", "treble", "volume"})
        {
            rows.push_back({std::string(knob) + "AtZero", knob, 0.0f});
            rows.push_back({std::string(knob) + "AtOne", knob, 1.0f});
        }
        return rows;
    }
} // namespace

TEST(Medium_DiezelVh4, DiezelVh4ProducesOnlyFiniteOutputOverManyBlocks)
{
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    node.set_gain(0.6f);
    node.set_master(0.0f);
    expect_all_finite(stream_sine(node, 220.0, 0.2, 20));
}

TEST(Small_DiezelVh4, DiezelVh4DescriptorListsTheParametersInContractOrder)
{
    // The index order is the contract of hosts and presets. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    ASSERT_EQ(descriptor.parameter_count, 10u);
    for (std::size_t i = 0; i < 10; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, K_PARAMETERS[i].id) << i;
    }
}

TEST(Small_DiezelVh4, DiezelVh4DescriptorGivesTheFirstSevenParametersLinearKindAndMidDefault)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    for (std::size_t i = 0; i < 7; ++i)
    {
        EXPECT_EQ(descriptor.parameters[i].kind, SushiDSP::ParamKind::Linear) << i;
        EXPECT_EQ(descriptor.parameters[i].default_value, 0.5f) << i;
    }
}

TEST(Small_DiezelVh4, DiezelVh4DescriptorGivesTheLevelParametersDecibelKindAndRange)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::diezel_vh4_descriptor();
    for (const DecibelRow& row : K_DECIBEL_ROWS)
    {
        EXPECT_EQ(descriptor.parameters[row.index].kind, SushiDSP::ParamKind::Decibel) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].min, row.min) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].max, row.max) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].default_value, row.default_value) << row.index;
    }
}

TEST(Small_DiezelVh4, DiezelVh4IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::DiezelVh4 node;
    for (std::uint16_t i = 0; i < 10; ++i)
    {
        node.set_parameter_for_audio_thread(i, K_PARAMETERS[i].value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_DiezelVh4, DiezelVh4IndexOutOfRangeChangesNoParameter)
{
    SushiDSP::DiezelVh4 node;
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

TEST(Small_DiezelVh4, DiezelVh4DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::DiezelVh4 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_DiezelVh4, DiezelVh4GenericParameterRoundTrip)
{
    SushiDSP::DiezelVh4 node;
    for (const ParameterRow& row : K_PARAMETERS)
    {
        node.set_parameter_for_control_thread(row.id, row.value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_DiezelVh4, DiezelVh4UnknownParameterIsSafeNoOp)
{
    SushiDSP::DiezelVh4 node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

class Medium_DiezelVh4Settled : public ::testing::TestWithParam<KnobExtreme>
{
};

TEST_P(Medium_DiezelVh4Settled, IsDcStableAcrossManyBlocksAtHighGain)
{
    // Each knob visits both extremes in turn: ten settings. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const KnobExtreme& row = GetParam();
    SushiDSP::DiezelVh4 node;
    node.set_parameter_for_control_thread(row.knob, row.value);
    node.set_master(unity_master_db);
    prepare_node(node);

    const std::vector<float> output = stream_sine(node, 220.0, 0.2, 200);
    expect_all_finite(output);
    EXPECT_LT(std::abs(mean_from(output, 100 * K_BLOCK)), K_RESIDUAL_DC_BOUND);
}

INSTANTIATE_TEST_SUITE_P(Extremes, Medium_DiezelVh4Settled, ::testing::ValuesIn(knob_extremes()),
                         [](const ::testing::TestParamInfo<KnobExtreme>& info)
                         { return info.param.name; });

TEST(Medium_DiezelVh4, DiezelVh4ThdDoesNotFallAsGainRises)
{
    // The THD reading must not fall as the gain knob rises. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const double low = thd_percent_at(0.1f, 0.2f);
    const double middle = thd_percent_at(0.5f, 0.2f);
    const double high = thd_percent_at(1.0f, 0.2f);

    EXPECT_GT(middle, low - K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS);
    EXPECT_GT(high, middle - K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS);
    EXPECT_GT(high, K_SATURATED_THD_FLOOR_PERCENT);
}

TEST(Medium_DiezelVh4, DiezelVh4NeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    node.set_parameter_for_control_thread("gain", 1.0f);
    node.set_parameter_for_control_thread("master", 12.0f);
    node.set_parameter_for_control_thread("output", 12.0f);

    const std::vector<float> output = stream_sine(node, 220.0, 1.0, 50);
    EXPECT_EQ(peak_from(output, 0), 1.0f);
}

TEST(Medium_DiezelVh4, DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    // The first blocks hold the startup transient. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    const std::vector<float> output = stream_sine(node, 82.0, 0.251, 188);
    const double peak_decibels = 20.0 * std::log10(static_cast<double>(peak_from(output, 21 * K_BLOCK)));
    EXPECT_NEAR(peak_decibels, K_CALIBRATION_PEAK_DBFS, K_CALIBRATION_BAND_DB);
}

TEST(Medium_DiezelVh4, DiezelVh4HasACleanEndAtMinimumGain)
{
    // The knob is read at 0.01 because 0.0 is a short. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    EXPECT_LT(thd_percent_at(0.01f, 0.01f), K_CLEAN_END_THD_CEILING_PERCENT); // -40 dBFS
}

TEST(Small_DiezelVh4, DiezelVh4GridProbesReadZeroAtRestForEveryStage)
{
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    ASSERT_EQ(node.triode_stage_count(), std::size_t{5});
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Medium_DiezelVh4, DiezelVh4GridProbesMoveForEveryStageAfterAudio)
{
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    stream_sine(node, 1000.0, 0.251, 40);
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Small_DiezelVh4, DiezelVh4GridProbeOutOfRangeReadsZero)
{
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
    EXPECT_EQ(node.grid_excursion_peak_volts(std::size_t{1000}), 0.0f);
}

TEST(Medium_DiezelVh4, DiezelVh4ResetClearsTheGridProbes)
{
    SushiDSP::DiezelVh4 node;
    prepare_node(node);
    stream_sine(node, 1000.0, 0.251, 40);
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Small_DiezelVh4, DiezelVh4DryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    prepare_node(node);

    SushiDSP::Oversampler8x reference(K_BLOCK);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Medium_DiezelVh4, DiezelVh4GainKnobMovesTheInputHighPassCorner)
{
    // The knob's travel is worth 3.0 dB at 300 Hz. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    auto response_for = [](float gain)
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(gain);
        node.set_master(unity_master_db);
        return SushiDSP::measure::frequency_response(node, K_SAMPLE_RATE, {300.0, 1000.0});
    };

    const auto lowest = response_for(0.02f);
    const auto highest = response_for(1.0f);

    const double low_knob_tilt = magnitude_at(lowest, 300.0) - magnitude_at(lowest, 1000.0);
    const double high_knob_tilt = magnitude_at(highest, 300.0) - magnitude_at(highest, 1000.0);
    const double analytic_travel = high_pass_tilt_db(K_HIGH_PASS_CORNER_HIGH_KNOB_HZ) -
                                   high_pass_tilt_db(K_HIGH_PASS_CORNER_LOW_KNOB_HZ);
    EXPECT_GT(high_knob_tilt - low_knob_tilt, analytic_travel - K_TRAVEL_SLACK_DB);
}

TEST(Medium_DiezelVh4, DiezelVh4DeepAtMaximumIsATenDecibelShelfAtEightyHertz)
{
    // A shelf carries half its decibels at its corner. See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    auto response_for = [](float deep)
    {
        SushiDSP::DiezelVh4 node;
        node.set_deep(deep);
        node.set_master(unity_master_db - 20.0f);
        return SushiDSP::measure::frequency_response(node, K_SAMPLE_RATE, {80.0});
    };

    const double lift_at_eighty = magnitude_at(response_for(1.0f), 80.0) - magnitude_at(response_for(0.0f), 80.0);
    EXPECT_NEAR(lift_at_eighty, K_DEEP_SHELF_DB / 2.0, K_SHELF_CORNER_BAND_DB);
}

TEST(Medium_DiezelVh4Characterization, MixBusAttenuationIsApplied)
{
    // The chain is linear end to end at -120 dBFS; the power block has no derivation yet (RW-89). See tests/nodes/diezel_vh4/test_diezel_vh4/README.md.
    SushiDSP::DiezelVh4 node;
    node.set_gain(0.5f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_volume(0.5f);
    node.set_deep(0.0f);
    node.set_presence(0.5f);
    node.set_master(unity_master_db);

    const double frequency = SushiDSP::measure::bin_exact_frequency(
        1000.0, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
    constexpr float amplitude = 0.000001f; // -120 dBFS
    const std::vector<float> rendered = SushiDSP::measure::render(
        node,
        SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                         K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE, amplitude),
        K_SAMPLE_RATE, K_BLOCK);

    const double peak = peak_from(rendered, rendered.size() / 2);
    EXPECT_NEAR(20.0 * std::log10(peak / amplitude), K_CHARACTERIZED_SMALL_SIGNAL_GAIN_DB,
                K_SMALL_SIGNAL_GAIN_BAND_DB);
}
