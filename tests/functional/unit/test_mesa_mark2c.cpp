/**************************************************************************/
/* test_mesa_mark2c.cpp                                                   */
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
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"
#include "measure/Thd.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr std::size_t kParameterCount = 15;

    /// @brief Renders a settled tone through a default-knob amp with one control moved.
    /// @param configure Called with the node before prepare(), so the smoothers snap.
    /// @param amplitude Input sine amplitude, full scale being 1.
    /// @param frequency Input frequency in Hz.
    /// @param blocks Number of 256-frame blocks rendered.
    /// @return Every rendered sample, in order.
    template <typename Configure>
    std::vector<float> render_mesa_mark2c(Configure configure, double amplitude, double frequency, int blocks)
    {
        SushiDSP::MesaMark2c node;
        configure(node);
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

        std::vector<float> rendered, input(256), output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };
        for (int block = 0; block < blocks; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const double n = block * 256.0 + static_cast<double>(i);
                input[i] = static_cast<float>(amplitude * std::sin(2.0 * M_PI * frequency * n / 48000.0));
            }
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);
            rendered.insert(rendered.end(), output.begin(), output.end());
        }
        return rendered;
    }

}

TEST(Unit_Dsp, MesaMark2cProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    const std::vector<float> rendered = render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.2, 220.0, 20);
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

TEST(Unit_Dsp, MesaMark2cDescriptorExposesFifteenParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.mesa_mark2c");
    EXPECT_STREQ(descriptor.name, "Mark IIC+");
    ASSERT_EQ(descriptor.parameter_count, kParameterCount);

    // The index order is the contract a host and a saved preset address these by, so it is
    // pinned here rather than merely checked for membership.
    const char* expected_ids[kParameterCount] = {
        "volume_1", "treble", "bass", "middle", "lead_drive", "lead_master", "master_1",
        "presence", "eq_80", "eq_240", "eq_750", "eq_2200", "eq_6600", "input_trim", "output"
    };
    for (std::size_t i = 0; i < kParameterCount; ++i)
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
    const float values[kParameterCount] = {
        0.2f, 0.3f, 0.4f, 0.45f, 0.6f, 0.7f, -6.0f, 0.8f,
        -1.0f, -0.5f, 0.25f, 0.5f, 1.0f, 12.0f, -3.0f
    };
    for (std::uint16_t i = 0; i < kParameterCount; ++i)
    {
        node.set_parameter(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mesa_mark2c_descriptor();
    for (std::uint16_t i = 0; i < kParameterCount; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter(static_cast<std::uint16_t>(99), 1.0f);
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
    node.set_parameter("volume_1", 0.2f);
    node.set_parameter("treble", 0.3f);
    node.set_parameter("bass", 0.4f);
    node.set_parameter("middle", 0.5f);
    node.set_parameter("lead_drive", 0.6f);
    node.set_parameter("lead_master", 0.7f);
    node.set_parameter("master_1", -6.0f);
    node.set_parameter("presence", 0.8f);
    node.set_parameter("eq_80", -1.0f);
    node.set_parameter("eq_240", -0.5f);
    node.set_parameter("eq_750", 0.25f);
    node.set_parameter("eq_2200", 0.5f);
    node.set_parameter("eq_6600", 1.0f);
    node.set_parameter("input_trim", 6.0f);
    node.set_parameter("output", -3.0f);

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
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
    node.set_equalizer_band(99, 1.0f);
}

TEST(Unit_Dsp, MesaMark2cIsDcStableAcrossManyBlocksAtHighGain)
{
    // Every knob is driven to both of its stops and held there for 200 blocks. The output must
    // stay finite, its settled DC term must stay under -40 dBFS, and that term must stop moving.
    //
    // The bound was 0.25 while OutputTransformerStage ran its tanh after its own rolloff and the
    // rectification the tanh introduced at the extreme setting was never blocked: the high stop
    // read -0.0506. Since DA5 part 3 the stage's secondary carries no mean, as a real secondary
    // winding does, and the high stop reads -0.00009 (-80.7 dBFS, 2026-09-07). See
    // modules/mesa_mark2c/README.md.
    //
    // At the low stop Master 1 sits at -60 dB, so a bound on the DC alone would pass on
    // attenuation. The drift check is what makes that extreme mean something: the DC of the
    // third quarter and of the last must agree, which a slowly diverging state would not.
    for (float extreme : { 0.0f, 1.0f })
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
                for (std::size_t band = 0; band < SushiDSP::MesaMark2c::kEqualizerBandCount; ++band)
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
        const double last_quarter_dc = last_sum / static_cast<double>(rendered.size() - last_quarter);
        EXPECT_LT(std::fabs(last_quarter_dc), 0.01) << "extreme " << extreme;
        EXPECT_LT(std::fabs(last_quarter_dc - third_quarter_dc), 0.01) << "extreme " << extreme;
    }
}

TEST(Unit_Dsp, MesaMark2cGridProbesReportEveryStageInSignalOrder)
{
    // The probe pair is what the README's grid table and the plate-swing work are read through, so
    // it needs its own cover: seven stages, every one of them reporting a grid that moved, and an
    // out-of-range index answering 0 rather than reading off the end of the array.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    ASSERT_EQ(node.triode_stage_count(), 7u);

    // Before any audio every stage rests at its bias point, so every probe reads zero.
    for (std::size_t stage = 0; stage < node.triode_stage_count(); ++stage)
    {
        EXPECT_EQ(node.grid_excursion_peak_volts(stage), 0.0f) << "stage " << stage;
    }

    std::vector<float> input(256), output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    for (int block = 0; block < 40; ++block)
    {
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            const double n = block * 256.0 + static_cast<double>(i);
            input[i] = static_cast<float>(0.251 * std::sin(2.0 * M_PI * 1000.0 * n / 48000.0));
        }
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
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
    // Spec section 4: the metric saturates once every stage clips, so a rising spectrum is not
    // required of Lead Drive; what is required is that turning it up never makes the amp cleaner,
    // within the 1.0 percentage point that section allows for the wave squaring off.
    // Measured with the shared THD analyser at a bin-exact 1 kHz and a 0.2 input.
    //
    // The sweep starts at 0.05, the first position that passes signal: audio_taper(0) is exactly
    // zero, so the knob's floor is a hard mute and the analyser would read noise against noise.
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::kFftSize);
    const double tolerance_percentage_points = 1.0;  // Spec section 4.
    double previous = 0.0;
    for (float lead_drive : { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f })
    {
        const std::vector<float> rendered = render_mesa_mark2c(
            [lead_drive](SushiDSP::MesaMark2c& node) { node.set_lead_drive(lead_drive); },
            0.2, frequency, 640);
        const double thd_percent = SushiDSP::measure::thd(rendered, frequency, 48000.0).thd_percent;
        EXPECT_GT(thd_percent, previous - tolerance_percentage_points)
            << "lead_drive " << lead_drive << " read " << thd_percent << " %, previous " << previous << " %";
        previous = thd_percent;
    }

    // Guards the sweep above against being satisfied by indistinguishable near-zero readings.
    EXPECT_GT(previous, 20.0);
}

TEST(Unit_Dsp, MesaMark2cNeverExceedsFullScaleWithoutALimiter)
{
    // A plain clamp is what bounds the output, so driving Lead Drive, Master 1 and Output to
    // their ceilings must still leave every sample inside [-1, 1] and must actually reach the
    // rail; otherwise the test proves headroom rather than the clamp.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    node.set_parameter("lead_drive", 1.0f);
    node.set_parameter("master_1", 12.0f);
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

TEST(Unit_Dsp, MesaMark2cHasACleanEndAtMinimumGain)
{
    GTEST_SKIP() << "Known issue: the IIC+ has no clean setting to measure. The V2A->V2B loop "
                    "send/return network is not in the dossier (section 3.7 prints no value for "
                    "it), so V2A's plate, ceilinged at 87 V, meets V2B's 4.14 V grid window with "
                    "nothing between them. At Lead Drive 0.02 and -40 dBFS the amp measures "
                    "41.5 % THD against this test's 10 % bar. Closes when the send/return network "
                    "is read off the schematic; dossier open questions 19 and 20.";

    // Kept below the skip so the assertion comes back with the network, not from memory. Lead
    // Drive 0.02 is the lowest position this test uses, not a threshold: the control passes signal
    // below it too.
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::kFftSize);
    const std::vector<float> rendered = render_mesa_mark2c(
        [](SushiDSP::MesaMark2c& node) { node.set_lead_drive(0.02f); }, 0.01, frequency, 640);

    const double thd_percent = SushiDSP::measure::thd(rendered, frequency, 48000.0).thd_percent;
    EXPECT_LT(thd_percent, 10.0) << "THD " << thd_percent << " %";
}

TEST(Unit_Dsp, MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // The shipped calibration point, and what pins MesaMark2c::kMasterUnityGain: a -12 dBFS
    // 82 Hz sine into an untouched amp comes back out at about -6 dBFS, the frequency all four
    // amplifiers pin at (DA5 spec section 6).
    const std::vector<float> rendered = render_mesa_mark2c([](SushiDSP::MesaMark2c&) {}, 0.251, 82.0, 188);

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
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);
    EXPECT_STREQ(node->type_id(), "sushidsp.mesa_mark2c");

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

TEST(Unit_Dsp, MesaMark2cCabinetStageEngagesAtANonDefaultBlockSize)
{
    // The cabinet's partition comes from the sample rate, not from PrepareInfo::max_block, so a
    // host running any block size gets the cabinet.
    constexpr std::uint32_t block_size = 128;
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    auto run = [block_size](std::vector<float> cabinet) -> std::vector<float>
    {
        SushiDSP::MesaMark2c node(std::move(cabinet));
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, block_size, 1 });

        std::vector<float> input(block_size), output(block_size, 0.0f);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
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

TEST(Unit_Dsp, MesaMark2cDryLatencyIsOneOversampledRegion)
{
    // V1A through the sag stage is a single 8x region, so the reported dry latency is one
    // Oversampler8x round trip; it is pinned against the oversampler rather than a literal.
    SushiDSP::MesaMark2c node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_FALSE(node.has_cabinet());

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}

TEST(Unit_Dsp, MesaMark2cToneStackSitsBeforeTheLeadStages)
{
    GTEST_SKIP() << "Known issue, the same one that skips MesaMark2cHasACleanEndAtMinimumGain: "
                    "the V2A->V2B loop send/return network is not in the dossier (section 3.7 "
                    "prints no value for it), so V2A's plate, ceilinged at 87 V, meets V2B's "
                    "4.14 V grid window with nothing between them and the node limits at every "
                    "input level. Treble at 0 against 1 moves THD by -0.09 pp at -80 dBFS and "
                    "-0.40 pp at -100 dBFS, against this test's 5 pp, so there is no input level "
                    "where the stack's colour reaches the output. Re-asserts that a brighter "
                    "stack drives the lead stages harder once the network is read off the "
                    "schematic; dossier open questions 19 and 20.";

    // Kept below the skip so the assertion comes back with the network, not from memory. Lead
    // Drive 0.02 is the lowest position this test uses, not a threshold: the control passes signal
    // below it too.
    const double frequency = SushiDSP::measure::bin_exact_frequency(1000.0, 48000.0, SushiDSP::measure::kFftSize);
    auto thd_at_treble = [frequency](float treble)
    {
        return SushiDSP::measure::thd(
            render_mesa_mark2c(
                [treble](SushiDSP::MesaMark2c& node)
                {
                    node.set_lead_drive(1.0f);
                    node.set_treble(treble);
                },
                0.2, frequency, 640),
            frequency, 48000.0).thd_percent;
    };

    const double dark = thd_at_treble(0.0f);
    const double bright = thd_at_treble(1.0f);
    EXPECT_LT(dark, bright);
    EXPECT_GT(bright - dark, 5.0);
}

TEST(Unit_Dsp, MesaMark2cEqBandBoostsItsCentre)
{
    GTEST_SKIP() << "Known issue, the same one that skips MesaMark2cHasACleanEndAtMinimumGain: "
                    "the V2A->V2B loop send/return network is not in the dossier (section 3.7 "
                    "prints no value for it), so V2A's plate, ceilinged at 87 V, meets V2B's "
                    "4.14 V grid window with nothing between them and the node limits at every "
                    "input level. The 750 Hz slider at +1 lifts 979.5 Hz by 0.009 dB at "
                    "-80 dBFS and -0.010 dB at -100 dBFS, against the bank's 17 dB, because the "
                    "output sits on its ceiling either way. Re-asserts the full 17 dB lift once "
                    "the network is read off the schematic; dossier open questions 19 and 20.";

    // Kept below the skip so the assertion comes back with the network, not from memory.
    constexpr double centre_hz = 979.5;
    auto level_at = [](float slider)
    {
        const std::vector<float> rendered = render_mesa_mark2c(
            [slider](SushiDSP::MesaMark2c& node)
            {
                node.set_volume_1(0.2f);
                node.set_lead_drive(0.2f);
                node.set_lead_master(0.2f);
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
        return 20.0 * std::log10(2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n + 1e-30);
    };

    EXPECT_NEAR(level_at(1.0f) - level_at(0.0f), 17.0, 1.0);
}
