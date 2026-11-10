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
 * @brief Tests the MesaMark2c node's parameters, levels, distortion, equalizer and switches.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using Configure = std::function<void(SushiDSP::MesaMark2c&)>;

    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256u;
    constexpr std::size_t K_PARAMETER_COUNT = 18;
    constexpr std::size_t K_KNOB_COUNT = 15;

    /// @brief Samples the settled-output tests skip before they measure: three times the 21-block
    /// startup transient of the filters and the sag stage, rounded to a power of two. Characterized.
    constexpr std::size_t K_SETTLE_SAMPLES = 16384u;

    /// @brief The residual DC the settled output may carry, in full-scale units (-40 dBFS).
    /// Characterized: the high stop reads -0.00009 (README).
    constexpr double K_RESIDUAL_DC_BOUND = 0.01;

    /// @brief How far the DC of the third quarter of a render may sit from the last quarter's,
    /// in full-scale units. Characterized: a slowly diverging state would exceed it.
    constexpr double K_DC_DRIFT_BOUND = 0.01;

    /// @brief How far THD may fall while Lead Drive rises, in percentage points (spec section 4).
    constexpr double K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS = 1.0;

    /// @brief THD the top of Lead Drive must exceed, in percent. Characterized: the first
    /// version of the test used this figure.
    constexpr double K_SATURATED_THD_FLOOR_PERCENT = 20.0;

    /// @brief THD the bottom of Lead Drive must stay under, in percent. Characterized: the
    /// reading is 0.20 % (README).
    constexpr double K_CLEAN_END_THD_CEILING_PERCENT = 10.0;

    /// @brief The level a -12 dBFS 82 Hz sine reaches at default settings, in dBFS (DA5 spec
    /// section 6).
    constexpr double K_CALIBRATION_PEAK_DBFS = -6.0;

    /// @brief How far the calibration peak may sit from the figure above, in dB. Characterized:
    /// the first version of the test used a window of -7 to -5 dBFS.
    constexpr double K_CALIBRATION_BAND_DB = 1.0;

    /// @brief How much more THD a full treble must give than a closed one, in percentage points.
    /// Characterized: the reading is +3.32 pp (README).
    constexpr double K_TREBLE_THD_RISE_FLOOR_PERCENTAGE_POINTS = 2.5;

    /// @brief The 750 Hz slider's full lift, in dB at the band's centre. Characterized: the
    /// reading is 16.93 dB at Master 1 -20 dB (README).
    constexpr double K_EQ_BAND_LIFT_DB = 17.0;

    /// @brief How far the lift may sit from the figure above, in dB. Characterized.
    constexpr double K_EQ_BAND_LIFT_BAND_DB = 1.0;

    /// @brief The centre of the 750 Hz band, in hertz (README of the module).
    constexpr double K_EQ_BAND_CENTRE_HZ = 723.43;

    /// @brief How far the output peak after the drive stops may fall relative to the first block
    /// after it, as a ratio. Characterized: the loop that oscillated read 0.92.
    constexpr double K_TAIL_DECAY_RATIO = 1e-3;

    /// @brief How far a block-size change may move the output, as a share of the largest sample.
    /// Characterized: rebuilding per block moved it by up to a whole block.
    constexpr double K_BLOCK_SIZE_AGREEMENT = 1e-3;

    /// @brief How deep V2B's cathode shelf is (README of the module): the factor by which the
    /// Deep switch changes the gain at 80 Hz.
    constexpr double K_V2B_SHELF_RATIO = 0.6972;

    /// @brief How far the Deep switch's shift at 80 Hz may sit from the shelf's, in dB.
    /// Characterized: the first version of the test used 0.4.
    constexpr double K_DEEP_SHIFT_BAND_DB = 0.4;

    /// @brief How far the Deep switch may move the gain at 3 kHz, in dB. Characterized.
    constexpr double K_DEEP_HIGH_BAND_DB = 0.1;

    /// @brief How far two equivalent equalizer positions may differ in gain, in dB. Characterized.
    constexpr double K_EQUIVALENT_POSITION_BAND_DB = 0.01;

    /// @brief How much an engaged 80 Hz slider must lift the gain at 80 Hz over a cut one, in dB.
    /// Characterized: the first version of the test used 6.
    constexpr double K_SLIDER_LIFT_FLOOR_DB = 6.0;

    /// @brief How much more the Bright switch must lift 6 kHz than 200 Hz, in dB. Characterized.
    constexpr double K_BRIGHT_TILT_FLOOR_DB = 1.0;

    /// @brief Builds a node, runs `configure` on it before prepare so the smoothers snap, and
    /// renders a sine of `blocks` blocks through it.
    /// @param configure Knob and switch settings.
    /// @param amplitude Input sine amplitude, full scale being 1.
    /// @param frequency Input frequency in Hz.
    /// @param blocks Number of K_BLOCK-frame blocks.
    std::vector<float> render_mesa_mark2c(const Configure& configure, double amplitude, double frequency,
                                          std::size_t blocks)
    {
        SushiDSP::MesaMark2c node;
        configure(node);
        return SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE, blocks * K_BLOCK,
                                             static_cast<float>(amplitude)),
            K_SAMPLE_RATE, K_BLOCK);
    }

    /// @brief THD in percent of a bin-exact sine after the startup transient.
    double thd_percent_at(const Configure& configure, double amplitude, double frequency_hz)
    {
        const double frequency = SushiDSP::measure::bin_exact_frequency(
            frequency_hz, K_SAMPLE_RATE, SushiDSP::measure::K_FFT_SIZE);
        SushiDSP::MesaMark2c node;
        configure(node);
        const std::vector<float> rendered = SushiDSP::measure::render(
            node,
            SushiDSP::measure::sine_stimulus(frequency, K_SAMPLE_RATE,
                                             K_SETTLE_SAMPLES + SushiDSP::measure::K_FFT_SIZE,
                                             static_cast<float>(amplitude)),
            K_SAMPLE_RATE, K_BLOCK);
        return SushiDSP::measure::thd(rendered, frequency, K_SAMPLE_RATE).thd_percent;
    }

    /// @brief Fails for every sample that is not a finite number.
    void expect_all_finite(const std::vector<float>& samples)
    {
        for (std::size_t n = 0; n < samples.size(); ++n)
        {
            ASSERT_TRUE(std::isfinite(samples[n])) << n;
        }
    }

    /// @brief Largest magnitude from `begin` up to `end`.
    float peak_between(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        float peak = 0.0f;
        for (std::size_t n = begin; n < end; ++n)
        {
            peak = std::max(peak, std::fabs(samples[n]));
        }
        return peak;
    }

    /// @brief Mean from `begin` up to `end`.
    double mean_between(const std::vector<float>& samples, std::size_t begin, std::size_t end)
    {
        double sum = 0.0;
        for (std::size_t n = begin; n < end; ++n)
        {
            sum += samples[n];
        }
        return sum / static_cast<double>(end - begin);
    }

    /// @brief Amplitude of the component at `frequency_hz` from `begin` to the end, by quadrature.
    double component_amplitude(const std::vector<float>& samples, double frequency_hz, std::size_t begin)
    {
        double sum_sin = 0.0;
        double sum_cos = 0.0;
        for (std::size_t i = begin; i < samples.size(); ++i)
        {
            const double phase = 2.0 * M_PI * frequency_hz * static_cast<double>(i) / K_SAMPLE_RATE;
            sum_sin += samples[i] * std::sin(phase);
            sum_cos += samples[i] * std::cos(phase);
        }
        return 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) /
               static_cast<double>(samples.size() - begin);
    }

    /// @brief Level in dBFS of the component at `frequency_hz` in the last quarter of a render.
    double level_db_at(const std::vector<float>& samples, double frequency_hz)
    {
        return 20.0 * std::log10(component_amplitude(samples, frequency_hz, samples.size() * 3 / 4));
    }

    /// @brief The amplifier's small-signal gain at one frequency, in decibels.
    ///
    /// The drive stays far below the level the peak test uses, so the chain stays inside its
    /// linear region and only the switch under test moves the answer.
    /// @param configure Applied before prepare(), to set the switch being measured.
    /// @param frequency_hz The drive frequency.
    double mark2c_gain_db(const Configure& configure, double frequency_hz)
    {
        constexpr double amplitude = 1e-4;
        const std::vector<float> rendered = render_mesa_mark2c(configure, amplitude, frequency_hz, 80);
        return 20.0 * std::log10(component_amplitude(rendered, frequency_hz, 50 * K_BLOCK) / amplitude);
    }

    /// @brief A configuration that engages or releases the Deep switch.
    Configure with_deep(bool engaged)
    {
        return [engaged](SushiDSP::MesaMark2c& node) { node.set_deep(engaged); };
    }

    /// @brief A configuration that engages or releases the Bright switch.
    Configure with_bright(bool engaged)
    {
        return [engaged](SushiDSP::MesaMark2c& node) { node.set_bright(engaged); };
    }

    /// @brief A configuration that sets the equalizer switch and the 80 Hz slider.
    Configure with_equalizer(SushiDSP::MesaMark2c::EqualizerMode mode, float slider)
    {
        return [mode, slider](SushiDSP::MesaMark2c& node)
        {
            node.set_equalizer_mode(mode);
            node.set_equalizer_band(0, slider);
        };
    }

    /// @brief Renders an 80 Hz tone under a slider sweep, in blocks of a given size.
    ///
    /// The slider is moved at the same absolute sample positions whatever the block size, so two
    /// renders differ only in how often the host hands the amplifier its work.
    /// @param block_frames The host block size to render in.
    /// @return 40 blocks' worth of output at the base rate.
    std::vector<float> render_equalizer_sweep(std::uint32_t block_frames)
    {
        constexpr std::uint32_t K_TOTAL_FRAMES = 40 * K_BLOCK;
        SushiDSP::MesaMark2c node;
        node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, block_frames, 1});

        std::vector<float> input(block_frames), block(block_frames, 0.0f), rendered;
        rendered.reserve(K_TOTAL_FRAMES);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {block.data()};

        for (std::uint32_t start = 0; start < K_TOTAL_FRAMES; start += block_frames)
        {
            if (start % K_BLOCK == 0)
            {
                node.set_equalizer_band(0, std::min(1.0f, static_cast<float>(start / K_BLOCK) / 10.0f));
            }
            for (std::uint32_t i = 0; i < block_frames; ++i)
            {
                const double n = static_cast<double>(start + i);
                input[i] = static_cast<float>(1e-4 * std::sin(2.0 * M_PI * 80.0 * n / K_SAMPLE_RATE));
            }
            SushiDSP::ProcessContext context{output_channels, input_channels, block_frames, 0.0};
            node.process(context);
            rendered.insert(rendered.end(), block.begin(), block.end());
        }
        return rendered;
    }

    /// @brief The largest absolute difference between two equally long renders.
    double largest_difference(const std::vector<float>& first, const std::vector<float>& second)
    {
        double largest = 0.0;
        for (std::size_t i = 0; i < first.size(); ++i)
        {
            largest = std::max(largest, std::fabs(static_cast<double>(first[i] - second[i])));
        }
        return largest;
    }

    /// @brief Names a parameter: its id and a value to set and read back, in descriptor order.
    struct ParameterRow
    {
            const char* id;
            float value;
    };

    /// @brief The eighteen parameters in the order hosts and presets address them by: the fifteen
    /// knobs, then the three switches. A switch reads back the position it selects, so each is
    /// given a value that is already one of its own positions.
    const ParameterRow K_PARAMETERS[K_PARAMETER_COUNT] = {
        {"volume_1", 0.2f},  {"treble", 0.3f},    {"bass", 0.4f},       {"middle", 0.45f},
        {"lead_drive", 0.6f}, {"lead_master", 0.7f}, {"master_1", -6.0f}, {"presence", 0.8f},
        {"eq_80", -1.0f},    {"eq_240", -0.5f},   {"eq_750", 0.25f},    {"eq_2200", 0.5f},
        {"eq_6600", 1.0f},   {"input_trim", 12.0f}, {"output", -3.0f},  {"bright", 1.0f},
        {"deep", 1.0f},      {"eq_mode", 2.0f}};

    /// @brief Names a decibel parameter: its descriptor index and range.
    struct DecibelRow
    {
            std::size_t index;
            float min;
            float max;
            float default_value;
    };

    const DecibelRow K_DECIBEL_ROWS[3] = {{6, -60.0f, 12.0f, 0.0f}, {13, -24.0f, 24.0f, 0.0f}, {14, -60.0f, 12.0f, 0.0f}};

    /// @brief Names one stop of every knob for the settled-DC rows.
    struct Extreme
    {
            const char* name;
            float knob;
            float master_db;
            float band;
    };

    const Extreme K_EXTREMES[2] = {{"AllKnobsDown", 0.0f, -60.0f, -1.0f}, {"AllKnobsUp", 1.0f, 12.0f, 1.0f}};

    /// @brief Names a pair of Lead Drive settings the THD must not fall between.
    struct DriveStep
    {
            const char* name;
            float lower;
            float upper;
    };

    const DriveStep K_DRIVE_STEPS[4] = {{"From0p05To0p1", 0.05f, 0.1f},
                                        {"From0p1To0p2", 0.1f, 0.2f},
                                        {"From0p2To0p5", 0.2f, 0.5f},
                                        {"From0p5To1", 0.5f, 1.0f}};

    /// @brief Names a Presence position for the tail rows.
    struct PresenceRow
    {
            const char* name;
            float presence;
    };

    const PresenceRow K_PRESENCE_ROWS[5] = {{"Presence0", 0.0f},
                                            {"Presence0p25", 0.25f},
                                            {"Presence0p5", 0.5f},
                                            {"Presence0p75", 0.75f},
                                            {"Presence1", 1.0f}};
} // namespace

TEST(Medium_MesaMark2c, MesaMark2cProducesOnlyFiniteOutputOverManyBlocks)
{
    expect_all_finite(render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.2, 220.0, 20));
}

TEST(Small_MesaMark2c, MesaMark2cDescriptorListsTheParametersInContractOrder)
{
    // The index order is the contract of hosts and presets. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    ASSERT_EQ(descriptor.parameter_count, K_PARAMETER_COUNT);
    for (std::size_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, K_PARAMETERS[i].id) << i;
    }
}

TEST(Small_MesaMark2c, MesaMark2cDescriptorGivesTheLevelParametersDecibelKindAndRange)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    for (const DecibelRow& row : K_DECIBEL_ROWS)
    {
        EXPECT_EQ(descriptor.parameters[row.index].kind, SushiDSP::ParamKind::Decibel) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].min, row.min) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].max, row.max) << row.index;
        EXPECT_EQ(descriptor.parameters[row.index].default_value, row.default_value) << row.index;
    }
}

TEST(Small_MesaMark2c, MesaMark2cDescriptorGivesTheEqualizerBandsLinearKindAndRange)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    for (std::size_t band = 8; band <= 12; ++band)
    {
        EXPECT_EQ(descriptor.parameters[band].kind, SushiDSP::ParamKind::Linear) << band;
        EXPECT_EQ(descriptor.parameters[band].min, -1.0f) << band;
        EXPECT_EQ(descriptor.parameters[band].max, 1.0f) << band;
        EXPECT_EQ(descriptor.parameters[band].default_value, 0.0f) << band;
    }
}

TEST(Small_MesaMark2c, MesaMark2cIndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::MesaMark2c node;
    for (std::uint16_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        node.set_parameter_for_audio_thread(i, K_PARAMETERS[i].value);
    }
    for (const ParameterRow& row : K_PARAMETERS)
    {
        EXPECT_EQ(node.get_parameter(row.id), row.value) << row.id;
    }
}

TEST(Small_MesaMark2c, MesaMark2cIndexOutOfRangeChangesNoParameter)
{
    SushiDSP::MesaMark2c node;
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

TEST(Small_MesaMark2c, MesaMark2cGenericParameterRoundTrip)
{
    SushiDSP::MesaMark2c node;
    for (std::size_t i = 0; i < K_KNOB_COUNT; ++i)
    {
        node.set_parameter_for_control_thread(K_PARAMETERS[i].id, K_PARAMETERS[i].value);
    }
    for (std::size_t i = 0; i < K_KNOB_COUNT; ++i)
    {
        EXPECT_EQ(node.get_parameter(K_PARAMETERS[i].id), K_PARAMETERS[i].value) << K_PARAMETERS[i].id;
    }
}

TEST(Small_MesaMark2c, MesaMark2cDecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::MesaMark2c node;
    node.set_master_1(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_EQ(node.get_parameter("master_1"), 12.0f);
    EXPECT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Small_MesaMark2c, MesaMark2cUnknownParameterIsSafeNoOp)
{
    SushiDSP::MesaMark2c node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_EQ(node.get_parameter("does_not_exist"), 0.0f);
    node.set_equalizer_band(99, 1.0f);
}

class Medium_MesaMark2cSettled : public ::testing::TestWithParam<Extreme>
{
};

TEST_P(Medium_MesaMark2cSettled, IsDcStableAcrossManyBlocksAtHighGain)
{
    // The settled DC stays under -40 dBFS and stops moving. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const Extreme& row = GetParam();
    const std::vector<float> rendered = render_mesa_mark2c(
        [&row](SushiDSP::MesaMark2c& node)
        {
            node.set_volume_1(row.knob);
            node.set_treble(row.knob);
            node.set_bass(row.knob);
            node.set_middle(row.knob);
            node.set_lead_drive(row.knob);
            node.set_lead_master(row.knob);
            node.set_presence(row.knob);
            node.set_master_1(row.master_db);
            for (std::size_t band = 0; band < SushiDSP::MesaMark2c::K_EQUALIZER_BAND_COUNT; ++band)
            {
                node.set_equalizer_band(band, row.band);
            }
        },
        0.1, 220.0, 200);
    expect_all_finite(rendered);

    const std::size_t third_quarter = rendered.size() / 2;
    const std::size_t last_quarter = rendered.size() * 3 / 4;
    const double last_dc = mean_between(rendered, last_quarter, rendered.size());
    EXPECT_LT(std::fabs(last_dc), K_RESIDUAL_DC_BOUND);
    EXPECT_LT(std::fabs(last_dc - mean_between(rendered, third_quarter, last_quarter)), K_DC_DRIFT_BOUND);
}

INSTANTIATE_TEST_SUITE_P(Extremes, Medium_MesaMark2cSettled, ::testing::ValuesIn(K_EXTREMES),
                         [](const ::testing::TestParamInfo<Extreme>& info)
                         { return std::string(info.param.name); });

TEST(Small_MesaMark2c, MesaMark2cReportsSevenTriodeStages)
{
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    EXPECT_EQ(node.triode_stage_count(), std::size_t{7});
}

TEST(Small_MesaMark2c, MesaMark2cGridProbesReadZeroAtRestForEveryStage)
{
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Medium_MesaMark2c, MesaMark2cGridProbesMoveForEveryStageAfterAudio)
{
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(1000.0, K_SAMPLE_RATE, 40 * K_BLOCK, 0.251f);
    std::vector<float> output(K_BLOCK, 0.0f);
    for (std::size_t block = 0; block < 40; ++block)
    {
        const float* input_channels[1] = {stimulus.data() + block * K_BLOCK};
        float* output_channels[1] = {output.data()};
        SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};
        node.process(context);
    }
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

TEST(Small_MesaMark2c, MesaMark2cGridProbeOutOfRangeReadsZero)
{
    // Seven stages report; an out-of-range index answers 0. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    EXPECT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);
}

TEST(Medium_MesaMark2c, MesaMark2cResetClearsTheGridProbes)
{
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});
    const std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(1000.0, K_SAMPLE_RATE, 40 * K_BLOCK, 0.251f);
    std::vector<float> output(K_BLOCK, 0.0f);
    for (std::size_t block = 0; block < 40; ++block)
    {
        const float* input_channels[1] = {stimulus.data() + block * K_BLOCK};
        float* output_channels[1] = {output.data()};
        SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};
        node.process(context);
    }
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << stage;
    }
}

class Medium_MesaMark2cThd : public ::testing::TestWithParam<DriveStep>
{
};

TEST_P(Medium_MesaMark2cThd, DoesNotFallAsLeadDriveRises)
{
    // The sweep starts at 0.05, where the knob passes signal. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const DriveStep& step = GetParam();
    const auto at_drive = [](float lead_drive)
    {
        return thd_percent_at([lead_drive](SushiDSP::MesaMark2c& node) { node.set_lead_drive(lead_drive); }, 0.2,
                              1000.0);
    };
    EXPECT_GT(at_drive(step.upper), at_drive(step.lower) - K_THD_FALL_TOLERANCE_PERCENTAGE_POINTS);
}

INSTANTIATE_TEST_SUITE_P(Steps, Medium_MesaMark2cThd, ::testing::ValuesIn(K_DRIVE_STEPS),
                         [](const ::testing::TestParamInfo<DriveStep>& info)
                         { return std::string(info.param.name); });

TEST(Medium_MesaMark2c, MesaMark2cThdAtMaximumLeadDriveIsSaturated)
{
    // Guards the sweep above against being satisfied by indistinguishable near-zero readings.
    EXPECT_GT(thd_percent_at([](SushiDSP::MesaMark2c& node) { node.set_lead_drive(1.0f); }, 0.2, 1000.0),
              K_SATURATED_THD_FLOOR_PERCENT);
}

TEST(Medium_MesaMark2c, MesaMark2cNeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const std::vector<float> rendered = render_mesa_mark2c(
        [](SushiDSP::MesaMark2c& node)
        {
            node.set_lead_drive(1.0f);
            node.set_master_1(12.0f);
            node.set_output(12.0f);
        },
        1.0, 220.0, 50);
    EXPECT_EQ(peak_between(rendered, 0, rendered.size()), 1.0f);
}

TEST(Medium_MesaMark2c, MesaMark2cHasACleanEndAtMinimumGain)
{
    // Lead Drive 0.02 at -40 dBFS reads 0.20 % THD. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    EXPECT_LT(thd_percent_at([](SushiDSP::MesaMark2c& node) { node.set_lead_drive(0.02f); }, 0.01, 1000.0),
              K_CLEAN_END_THD_CEILING_PERCENT);
}

TEST(Medium_MesaMark2c, MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const std::vector<float> rendered = render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.251, 82.0, 188);
    const double peak_decibels =
        20.0 * std::log10(static_cast<double>(peak_between(rendered, rendered.size() / 4, rendered.size())));
    EXPECT_NEAR(peak_decibels, K_CALIBRATION_PEAK_DBFS, K_CALIBRATION_BAND_DB);
}

TEST(Small_MesaMark2c, MesaMark2cDryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK, 1});

    SushiDSP::Oversampler8x reference(K_BLOCK);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Medium_MesaMark2c, MesaMark2cToneStackSitsBeforeTheLeadStages)
{
    // The stack is ahead of the distortion: treble moves THD. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const auto at_treble = [](float treble)
    {
        return thd_percent_at(
            [treble](SushiDSP::MesaMark2c& node)
            {
                node.set_lead_drive(0.05f);
                node.set_treble(treble);
            },
            0.01, 1000.0);
    };
    EXPECT_GT(at_treble(1.0f) - at_treble(0.0f), K_TREBLE_THD_RISE_FLOOR_PERCENTAGE_POINTS);
}

TEST(Medium_MesaMark2c, MesaMark2cEqBandBoostsItsCentre)
{
    // At Master 1 -20 dB the power section is linear. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const auto level_at = [](float slider)
    {
        return level_db_at(render_mesa_mark2c(
                               [slider](SushiDSP::MesaMark2c& node)
                               {
                                   node.set_volume_1(0.2f);
                                   node.set_lead_drive(0.2f);
                                   node.set_lead_master(0.2f);
                                   node.set_master_1(-20.0f);
                                   node.set_equalizer_band(2, slider);
                               },
                               0.005, K_EQ_BAND_CENTRE_HZ, 120),
                           K_EQ_BAND_CENTRE_HZ);
    };
    EXPECT_NEAR(level_at(1.0f) - level_at(0.0f), K_EQ_BAND_LIFT_DB, K_EQ_BAND_LIFT_BAND_DB);
}

class Medium_MesaMark2cTail : public ::testing::TestWithParam<PresenceRow>
{
};

TEST_P(Medium_MesaMark2cTail, FallsByThreeOrdersOfMagnitudeWhenTheDriveStops)
{
    // The ratio sat at 0.92 while the loop oscillated. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.set_presence(GetParam().presence);
    std::vector<float> stimulus =
        SushiDSP::measure::sine_stimulus(1000.0, K_SAMPLE_RATE, 8 * K_BLOCK, 1e-3f);
    stimulus.resize(48 * K_BLOCK, 0.0f);
    const std::vector<float> rendered = SushiDSP::measure::render(node, stimulus, K_SAMPLE_RATE, K_BLOCK);

    const double first = peak_between(rendered, 8 * K_BLOCK, 9 * K_BLOCK);
    const double last = peak_between(rendered, 47 * K_BLOCK, 48 * K_BLOCK);
    EXPECT_LT(last, first * K_TAIL_DECAY_RATIO);
}

INSTANTIATE_TEST_SUITE_P(Presence, Medium_MesaMark2cTail, ::testing::ValuesIn(K_PRESENCE_ROWS),
                         [](const ::testing::TestParamInfo<PresenceRow>& info)
                         { return std::string(info.param.name); });

TEST(Medium_MesaMark2c, MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize)
{
    // Coefficients are rebuilt once per control period. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const std::vector<float> in_256 = render_equalizer_sweep(256);
    const std::vector<float> in_32 = render_equalizer_sweep(32);
    ASSERT_EQ(in_256.size(), in_32.size());

    const double largest_sample = peak_between(in_256, 0, in_256.size());
    EXPECT_GT(largest_sample, 0.0);
    EXPECT_LT(largest_difference(in_256, in_32), largest_sample * K_BLOCK_SIZE_AGREEMENT);
}

TEST(Medium_MesaMark2c, MesaMark2cPullDeepStopsV2bRemovingBassAtEightyHertz)
{
    // V2B's cathode shelf is 0.6972 deep, -3.13 dB. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double shift = mark2c_gain_db(with_deep(true), 80.0) - mark2c_gain_db(with_deep(false), 80.0);
    EXPECT_NEAR(shift, -20.0 * std::log10(K_V2B_SHELF_RATIO), K_DEEP_SHIFT_BAND_DB);
}

TEST(Medium_MesaMark2c, MesaMark2cPullDeepDoesNothingAtThreeKilohertz)
{
    // At 3 kHz both settings are far above either zero. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double shift = mark2c_gain_db(with_deep(true), 3000.0) - mark2c_gain_db(with_deep(false), 3000.0);
    EXPECT_NEAR(shift, 0.0, K_DEEP_HIGH_BAND_DB);
}

TEST(Medium_MesaMark2c, MesaMark2cEqualizerOutTakesTheSlidersOutOfThePath)
{
    // With the bus lifted the 80 Hz slider reaches nothing. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    using Mode = SushiDSP::MesaMark2c::EqualizerMode;
    EXPECT_NEAR(mark2c_gain_db(with_equalizer(Mode::Out, 1.0f), 80.0),
                mark2c_gain_db(with_equalizer(Mode::Out, -1.0f), 80.0), K_EQUIVALENT_POSITION_BAND_DB);
}

TEST(Medium_MesaMark2c, MesaMark2cEqualizerInLetsTheSliderMoveItsBand)
{
    using Mode = SushiDSP::MesaMark2c::EqualizerMode;
    EXPECT_GT(mark2c_gain_db(with_equalizer(Mode::In, 1.0f), 80.0),
              mark2c_gain_db(with_equalizer(Mode::In, -1.0f), 80.0) + K_SLIDER_LIFT_FLOOR_DB);
}

TEST(Medium_MesaMark2c, MesaMark2cEqualizerAutoIsTheInPositionForALeadOnlyNode)
{
    using Mode = SushiDSP::MesaMark2c::EqualizerMode;
    EXPECT_NEAR(mark2c_gain_db(with_equalizer(Mode::Auto, 1.0f), 80.0),
                mark2c_gain_db(with_equalizer(Mode::In, 1.0f), 80.0), K_EQUIVALENT_POSITION_BAND_DB);
}

TEST(Medium_MesaMark2c, MesaMark2cPullBrightLiftsTrebleAtBothOfItsCapacitors)
{
    // The switch tilts the amplifier; it does not raise it. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double low_lift = mark2c_gain_db(with_bright(true), 200.0) - mark2c_gain_db(with_bright(false), 200.0);
    const double high_lift = mark2c_gain_db(with_bright(true), 6000.0) - mark2c_gain_db(with_bright(false), 6000.0);
    EXPECT_GT(high_lift, low_lift + K_BRIGHT_TILT_FLOOR_DB);
}
