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
 * @brief Tests the MesaMark2c node's parameters, levels, switches and latency.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <algorithm>
#include <cmath>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>
#include <SushiDSP/foundation/signal_measure/thd.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr std::size_t K_PARAMETER_COUNT = 18;

    /// @brief Renders a settled tone through a default-knob amp with one control moved.
    /// @param configure Called with the node before prepare(), so the smoothers snap.
    /// @param amplitude Input sine amplitude, full scale being 1.
    /// @param frequency Input frequency in Hz.
    /// @param blocks Number of 256-frame blocks rendered.
    /// @return Every rendered sample, in order.
    template <typename Configure>
    std::vector<float> render_mesa_mark2c(Configure configure, double amplitude, double frequency,
                                          int blocks)
    {
        SushiDSP::MesaMark2c node;
        configure(node);
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

        std::vector<float> rendered, input(256), output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        for (int block = 0; block < blocks; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double n = block * 256.0 + static_cast<double>(i);
                input[i] =
                    static_cast<float>(amplitude * std::sin(2.0 * M_PI * frequency * n / 48000.0));
            }
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);
            rendered.insert(rendered.end(), output.begin(), output.end());
        }
        return rendered;
    }

} // namespace

TEST(Unit_Dsp, MesaMark2cProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    const std::vector<float> rendered =
        render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.2, 220.0, 20);
    for (float sample : rendered)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, MesaMark2cPortsAreMonoInMonoOut)
{
    SushiDSP::MesaMark2c node;
    const SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, MesaMark2cDescriptorExposesEighteenParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.mesa_mark2c");
    EXPECT_STREQ(descriptor.name, "Mark IIC+");
    ASSERT_EQ(descriptor.parameter_count, K_PARAMETER_COUNT);

    // The index order is the contract of hosts and presets. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const char* expected_ids[K_PARAMETER_COUNT] = {
        "volume_1", "treble", "bass", "middle", "lead_drive", "lead_master", "master_1", "presence",
        "eq_80", "eq_240", "eq_750", "eq_2200", "eq_6600", "input_trim", "output",
        // The three switches follow the fifteen knobs. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
        "bright", "deep", "eq_mode"};
    for (std::size_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, expected_ids[i]);
    }

    EXPECT_EQ(descriptor.parameters[6].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[13].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[14].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].max, 12.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[13].min, -24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[13].max, 24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[14].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[14].max, 12.0f);

    for (std::size_t band = 8; band <= 12; ++band)
    {
        EXPECT_EQ(descriptor.parameters[band].kind, SushiDSP::ParamKind::Linear);
        EXPECT_FLOAT_EQ(descriptor.parameters[band].min, -1.0f);
        EXPECT_FLOAT_EQ(descriptor.parameters[band].max, 1.0f);
        EXPECT_FLOAT_EQ(descriptor.parameters[band].default_value, 0.0f);
    }
}

TEST(Unit_Dsp, MesaMark2cIndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::MesaMark2c node;
    const float values[K_PARAMETER_COUNT] = {
        0.2f, 0.3f, 0.4f, 0.45f, 0.6f, 0.7f, -6.0f, 0.8f, -1.0f, -0.5f, 0.25f, 0.5f, 1.0f, 12.0f,
        -3.0f,
        // A switch reads back the position it selects. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
        1.0f, 1.0f, 2.0f};
    for (std::uint16_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        node.set_parameter_for_audio_thread(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    for (std::uint16_t i = 0; i < K_PARAMETER_COUNT; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Unit_Dsp, MesaMark2cDecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::MesaMark2c node;
    node.set_master_1(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("master_1"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Unit_Dsp, MesaMark2cGenericParameterRoundTrip)
{
    SushiDSP::MesaMark2c node;
    node.set_parameter_for_control_thread("volume_1", 0.2f);
    node.set_parameter_for_control_thread("treble", 0.3f);
    node.set_parameter_for_control_thread("bass", 0.4f);
    node.set_parameter_for_control_thread("middle", 0.5f);
    node.set_parameter_for_control_thread("lead_drive", 0.6f);
    node.set_parameter_for_control_thread("lead_master", 0.7f);
    node.set_parameter_for_control_thread("master_1", -6.0f);
    node.set_parameter_for_control_thread("presence", 0.8f);
    node.set_parameter_for_control_thread("eq_80", -1.0f);
    node.set_parameter_for_control_thread("eq_240", -0.5f);
    node.set_parameter_for_control_thread("eq_750", 0.25f);
    node.set_parameter_for_control_thread("eq_2200", 0.5f);
    node.set_parameter_for_control_thread("eq_6600", 1.0f);
    node.set_parameter_for_control_thread("input_trim", 6.0f);
    node.set_parameter_for_control_thread("output", -3.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("volume_1"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("lead_drive"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("lead_master"), 0.7f);
    EXPECT_FLOAT_EQ(node.get_parameter("master_1"), -6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.8f);
    EXPECT_FLOAT_EQ(node.get_parameter("eq_80"), -1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("eq_240"), -0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("eq_750"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("eq_2200"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("eq_6600"), 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), -3.0f);
}

TEST(Unit_Dsp, MesaMark2cUnknownParameterIsSafeNoOp)
{
    SushiDSP::MesaMark2c node;
    node.set_parameter_for_control_thread("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
    node.set_equalizer_band(99, 1.0f);
}

TEST(Unit_Dsp, MesaMark2cIsDcStableAcrossManyBlocksAtHighGain)
{
    // The settled DC stays under -40 dBFS and stops moving. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    for (float extreme : {0.0f, 1.0f})
    {
        const std::vector<float> rendered = render_mesa_mark2c(
            [extreme](SushiDSP::MesaMark2c& node)
            {
                node.set_volume_1(extreme);
                node.set_treble(extreme);
                node.set_bass(extreme);
                node.set_middle(extreme);
                node.set_lead_drive(extreme);
                node.set_lead_master(extreme);
                node.set_presence(extreme);
                node.set_master_1(extreme > 0.5f ? 12.0f : -60.0f);
                for (std::size_t band = 0; band < SushiDSP::MesaMark2c::K_EQUALIZER_BAND_COUNT;
                     ++band)
                {
                    node.set_equalizer_band(band, extreme > 0.5f ? 1.0f : -1.0f);
                }
            },
            0.1, 220.0, 200);

        const std::size_t third_quarter = rendered.size() / 2;
        const std::size_t last_quarter = rendered.size() * 3 / 4;
        double third_sum = 0.0, last_sum = 0.0;
        for (std::size_t i = third_quarter; i < last_quarter; ++i)
        {
            ASSERT_TRUE(std::isfinite(rendered[i])) << "extreme " << extreme;
            third_sum += rendered[i];
        }
        for (std::size_t i = last_quarter; i < rendered.size(); ++i)
        {
            ASSERT_TRUE(std::isfinite(rendered[i])) << "extreme " << extreme;
            last_sum += rendered[i];
        }
        const double quarter = static_cast<double>(last_quarter - third_quarter);
        const double third_quarter_dc = third_sum / quarter;
        const double last_quarter_dc =
            last_sum / static_cast<double>(rendered.size() - last_quarter);
        EXPECT_LT(std::fabs(last_quarter_dc), 0.01) << "extreme " << extreme;
        EXPECT_LT(std::fabs(last_quarter_dc - third_quarter_dc), 0.01) << "extreme " << extreme;
    }
}

TEST(Unit_Dsp, MesaMark2cGridProbesReportEveryStageInSignalOrder)
{
    // Seven stages report; an out-of-range index answers 0. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    ASSERT_EQ(node.triode_stage_count(), 7u);

    // Before any audio every stage rests at its bias point, so every probe reads zero.
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    std::vector<float> input(256), output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    for (int block = 0; block < 40; ++block)
    {
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            const double n = block * 256.0 + static_cast<double>(i);
            input[i] = static_cast<float>(0.251 * std::sin(2.0 * M_PI * 1000.0 * n / 48000.0));
        }
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_GT(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
    EXPECT_FLOAT_EQ(node.grid_excursion_peak_volts(node.triode_stage_count()), 0.0f);

    // reset() clears the peaks with the rest of the state.
    node.reset();
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }
}

TEST(Unit_Dsp, MesaMark2cThdDoesNotFallAsLeadDriveRises)
{
    // The sweep starts at 0.05, where the knob passes signal. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    const double tolerance_percentage_points = 1.0; // Spec section 4.
    double previous = 0.0;
    for (float lead_drive : {0.05f, 0.1f, 0.2f, 0.5f, 1.0f})
    {
        const std::vector<float> rendered = render_mesa_mark2c(
            [lead_drive](SushiDSP::MesaMark2c& node)
            {
                node.set_lead_drive(lead_drive);
            },
            0.2, frequency, 640);
        const double thd_percent = SushiDSP::measure::thd(rendered, frequency, 48000.0).thd_percent;
        EXPECT_GT(thd_percent, previous - tolerance_percentage_points)
            << "lead_drive " << lead_drive << " read " << thd_percent << " %, previous " << previous
            << " %";
        previous = thd_percent;
    }

    // Guards the sweep above against being satisfied by indistinguishable near-zero readings.
    EXPECT_GT(previous, 20.0);
}

TEST(Unit_Dsp, MesaMark2cNeverExceedsFullScaleWithoutALimiter)
{
    // The output stays in [-1, 1] and reaches the rail. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    node.set_parameter_for_control_thread("lead_drive", 1.0f);
    node.set_parameter_for_control_thread("master_1", 12.0f);
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

TEST(Unit_Dsp, MesaMark2cHasACleanEndAtMinimumGain)
{
    // Lead Drive 0.02 at -40 dBFS reads 0.20 % THD. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    const std::vector<float> rendered = render_mesa_mark2c(
        [](SushiDSP::MesaMark2c& node)
        {
            node.set_lead_drive(0.02f);
        },
        0.01, frequency, 640);

    const double thd_percent = SushiDSP::measure::thd(rendered, frequency, 48000.0).thd_percent;
    EXPECT_LT(thd_percent, 10.0) << "THD " << thd_percent << " %";
}

TEST(Unit_Dsp, MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // A -12 dBFS 82 Hz sine comes out at about -6 dBFS. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const std::vector<float> rendered =
        render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.251, 82.0, 188);

    float peak = 0.0f;
    for (std::size_t i = rendered.size() / 4; i < rendered.size(); ++i)
    {
        peak = std::max(peak, std::fabs(rendered[i]));
    }

    const float peak_decibels = 20.0f * std::log10(peak);
    EXPECT_GT(peak_decibels, -7.0f);
    EXPECT_LT(peak_decibels, -5.0f);
}

TEST(Unit_Dsp, MesaMark2cDescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    SushiDSP::PrepareInfo info{48000.0, 256, 1};

    std::unique_ptr<SushiDSP::AudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);
    EXPECT_STREQ(node->type_id(), "sushidsp.mesa_mark2c");

    std::vector<float> input(256, 0.0f), output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
    node->process(context);

    for (float sample : output)
    {
        EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(Unit_Dsp, MesaMark2cDryLatencyIsOneOversampledRegion)
{
    // The latency is one Oversampler8x round trip. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Unit_Dsp, MesaMark2cToneStackSitsBeforeTheLeadStages)
{
    // The stack is ahead of the distortion: treble moves THD. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    const double frequency =
        SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::K_FFT_SIZE);
    auto thd_at_treble = [frequency](float treble)
    {
        return SushiDSP::measure::thd(render_mesa_mark2c(
                                          [treble](SushiDSP::MesaMark2c& node)
                                          {
                                              node.set_lead_drive(0.05f);
                                              node.set_treble(treble);
                                          },
                                          0.01, frequency, 640),
                                      frequency, 48000.0)
            .thd_percent;
    };

    const double dark = thd_at_treble(0.0f);
    const double bright = thd_at_treble(1.0f);
    EXPECT_LT(dark, bright);
    EXPECT_GT(bright - dark, 2.5);
}

TEST(Unit_Dsp, MesaMark2cEqBandBoostsItsCentre)
{
    // At Master 1 -20 dB the power section is linear. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    static constexpr double centre_hz = 723.43;
    auto level_at = [](float slider)
    {
        const std::vector<float> rendered = render_mesa_mark2c(
            [slider](SushiDSP::MesaMark2c& node)
            {
                node.set_volume_1(0.2f);
                node.set_lead_drive(0.2f);
                node.set_lead_master(0.2f);
                node.set_master_1(-20.0f);
                node.set_equalizer_band(2, slider);
            },
            0.005, centre_hz, 120);

        const std::size_t start = rendered.size() * 3 / 4;
        double sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = start; i < rendered.size(); ++i)
        {
            const double phase = 2.0 * M_PI * centre_hz * i / 48000.0;
            sum_sin += rendered[i] * std::sin(phase);
            sum_cos += rendered[i] * std::cos(phase);
        }
        const std::size_t n = rendered.size() - start;
        return 20.0 *
               std::log10(2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n + 1e-30);
    };

    EXPECT_NEAR(level_at(1.0f) - level_at(0.0f), 17.0, 1.0);
}

TEST(Unit_Dsp, MesaMark2cFallsSilentAtEveryPresencePositionWhenTheDriveStops)
{
    for (float presence : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        SushiDSP::MesaMark2c node;
        node.set_presence(presence);
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

        std::vector<float> input(256), output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        double first_silent_peak = 0.0, last_silent_peak = 0.0;
        for (int block = 0; block < 48; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double n = block * 256.0 + static_cast<double>(i);
                input[i] =
                    block < 8
                        ? static_cast<float>(1e-3 * std::sin(2.0 * M_PI * 1000.0 * n / 48000.0))
                        : 0.0f;
            }
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);

            double peak = 0.0;
            for (std::size_t i = 0; i < output.size(); ++i)
            {
                peak = std::max(peak, std::fabs(static_cast<double>(output[i])));
            }
            first_silent_peak = block == 8 ? peak : first_silent_peak;
            last_silent_peak = block == 47 ? peak : last_silent_peak;
        }

        // The ratio sat at 0.92 while the loop oscillated. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
        EXPECT_LT(last_silent_peak / (first_silent_peak + 1e-30), 1e-3) << "presence " << presence;
    }
}

namespace
{
    /// @brief Renders an 80 Hz tone under a slider sweep, in blocks of a given size.
    ///
    /// The slider is moved at the same absolute sample positions whatever the block size, so two
    /// renders differ only in how often the host hands the amplifier its work.
    /// @param block_frames The host block size to render in.
    /// @return 40 blocks' worth of output at the base rate.
    std::vector<float> render_equalizer_sweep(std::uint32_t block_frames)
    {
        constexpr std::uint32_t K_TOTAL_FRAMES = 40 * 256;
        SushiDSP::MesaMark2c node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, block_frames, 1});

        std::vector<float> input(block_frames), block(block_frames, 0.0f), rendered;
        rendered.reserve(K_TOTAL_FRAMES);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {block.data()};

        for (std::uint32_t start = 0; start < K_TOTAL_FRAMES; start += block_frames)
        {
            if (start % 256 == 0)
            {
                node.set_equalizer_band(0, std::min(1.0f, static_cast<float>(start / 256) / 10.0f));
            }
            for (std::uint32_t i = 0; i < block_frames; ++i)
            {
                const double n = static_cast<double>(start + i);
                input[i] = static_cast<float>(1e-4 * std::sin(2.0 * M_PI * 80.0 * n / 48000.0));
            }
            SushiDSP::ProcessContext context{output_channels, input_channels, block_frames, 0.0};
            node.process(context);
            rendered.insert(rendered.end(), block.begin(), block.end());
        }
        return rendered;
    }
} // namespace

TEST(Unit_Dsp, MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize)
{
    const std::vector<float> in_256 = render_equalizer_sweep(256);
    const std::vector<float> in_32 = render_equalizer_sweep(32);
    ASSERT_EQ(in_256.size(), in_32.size());

    double largest_difference = 0.0, largest_sample = 0.0;
    for (std::size_t i = 0; i < in_256.size(); ++i)
    {
        largest_difference =
            std::max(largest_difference, std::fabs(static_cast<double>(in_256[i] - in_32[i])));
        largest_sample = std::max(largest_sample, std::fabs(static_cast<double>(in_256[i])));
    }

    // Coefficients are rebuilt once per control period. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    EXPECT_GT(largest_sample, 0.0);
    EXPECT_LT(largest_difference, largest_sample * 1e-3);
}

namespace
{
    /// @brief Measures the amplifier's small-signal gain at one frequency.
    ///
    /// The drive stays far below the level the peak test uses, so the chain stays inside its
    /// linear region and only the switch under test moves the answer.
    /// @param configure Applied before prepare(), to set the switch being measured.
    /// @param frequency_hz The drive frequency.
    /// @return The gain in decibels.
    double mark2c_gain_db(const std::function<void(SushiDSP::MesaMark2c&)>& configure,
                          double frequency_hz)
    {
        SushiDSP::MesaMark2c node;
        configure(node);
        node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

        constexpr double amplitude = 1e-4;
        std::vector<float> input(256), output(256, 0.0f);
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};

        double real_part = 0.0, imaginary_part = 0.0;
        std::size_t counted = 0;
        for (int block = 0; block < 80; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double n = block * 256.0 + static_cast<double>(i);
                input[i] = static_cast<float>(amplitude *
                                              std::sin(2.0 * M_PI * frequency_hz * n / 48000.0));
            }
            SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
            node.process(context);
            if (block < 50)
            {
                continue;
            }
            for (std::size_t i = 0; i < output.size(); ++i)
            {
                const double n = block * 256.0 + static_cast<double>(i);
                const double phase = 2.0 * M_PI * frequency_hz * n / 48000.0;
                real_part += output[i] * std::sin(phase);
                imaginary_part += output[i] * std::cos(phase);
                ++counted;
            }
        }
        const double magnitude =
            2.0 * std::sqrt(real_part * real_part + imaginary_part * imaginary_part) /
            static_cast<double>(counted);
        return 20.0 * std::log10(magnitude / amplitude + 1e-30);
    }
} // namespace

TEST(Unit_Dsp, MesaMark2cPullDeepStopsV2bRemovingBass)
{
    const auto deep = [](bool engaged)
    {
        return [engaged](SushiDSP::MesaMark2c& node)
        {
            node.set_deep(engaged);
        };
    };

    // V2B's cathode shelf is 0.6972 deep, -3.13 dB. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    EXPECT_NEAR(mark2c_gain_db(deep(true), 80.0) - mark2c_gain_db(deep(false), 80.0), 3.13, 0.4);

    // At 3 kHz both settings are far above either zero, so the switch does nothing.
    EXPECT_NEAR(mark2c_gain_db(deep(true), 3000.0) - mark2c_gain_db(deep(false), 3000.0), 0.0, 0.1);
}

TEST(Unit_Dsp, MesaMark2cEqualizerOutTakesTheSlidersOutOfThePath)
{
    using Mode = SushiDSP::MesaMark2c::EqualizerMode;
    const auto with = [](Mode mode, float slider)
    {
        return [mode, slider](SushiDSP::MesaMark2c& node)
        {
            node.set_equalizer_mode(mode);
            node.set_equalizer_band(0, slider);
        };
    };

    // With the bus lifted the 80 Hz slider reaches nothing, so both extremes give one answer.
    EXPECT_NEAR(mark2c_gain_db(with(Mode::Out, 1.0f), 80.0),
                mark2c_gain_db(with(Mode::Out, -1.0f), 80.0), 0.01);

    // Engaged, the same slider moves the band it is wired to.
    EXPECT_GT(mark2c_gain_db(with(Mode::In, 1.0f), 80.0),
              mark2c_gain_db(with(Mode::In, -1.0f), 80.0) + 6.0);

    // Auto is the In position for a node that is only ever the lead channel.
    EXPECT_NEAR(mark2c_gain_db(with(Mode::Auto, 1.0f), 80.0),
                mark2c_gain_db(with(Mode::In, 1.0f), 80.0), 0.01);
}

TEST(Unit_Dsp, MesaMark2cPullBrightLiftsTrebleAtBothOfItsCapacitors)
{
    const auto bright = [](bool engaged)
    {
        return [engaged](SushiDSP::MesaMark2c& node)
        {
            node.set_bright(engaged);
        };
    };

    const double low_in = mark2c_gain_db(bright(true), 200.0);
    const double low_out = mark2c_gain_db(bright(false), 200.0);
    const double high_in = mark2c_gain_db(bright(true), 6000.0);
    const double high_out = mark2c_gain_db(bright(false), 6000.0);

    // The switch tilts the amplifier; it does not raise it. See tests/nodes/mesa_mark2c/test_mesa_mark2c/README.md.
    EXPECT_GT(high_in - high_out, low_in - low_out + 1.0);
}
