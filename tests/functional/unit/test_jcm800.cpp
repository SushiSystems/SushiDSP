/**************************************************************************/
/* test_jcm800.cpp                                                        */
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
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/io/Ir.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // The master level whose linear multiplier is exactly 1.0. Master is a dB control whose 0 dB
    // reference is kMasterUnityGain, so the tests that used to ask for "master fully up at 1.0
    // linear" ask for this instead and keep measuring the same signal - including across the D5b
    // recalibration of that reference, which is why it is derived from the constant rather than
    // written out as a number.
    const float kUnityMasterDb = -SushiDSP::gain_to_decibels(SushiDSP::Jcm800::kMasterUnityGain);
}

TEST(Unit_Dsp, Jcm800ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(0.0f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(0.5f);

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

TEST(Unit_Dsp, Jcm800OutputNeverExceedsFullScaleUnderHotGainStaging)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(1.0f);
    node.set_master(12.0f);
    node.set_bass(1.0f);
    node.set_middle(1.0f);
    node.set_treble(1.0f);
    node.set_presence(1.0f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 40; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
    }

    for (float sample : output)
    {
        EXPECT_LE(std::fabs(sample), 1.0f);
    }
}

TEST(Unit_Dsp, Jcm800PortsAreMonoInMonoOut)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, Jcm800MasterAtMinimumProducesSilenceAfterSettling)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(-60.0f);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(0.5f);

    std::vector<float> input(256, 0.3f);
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
        EXPECT_NEAR(sample, 0.0f, 1e-2f);
    }
}

TEST(Unit_Dsp, Jcm800PreampIncreasesOutputEnergy)
{
    auto measure_rms = [](float preamp) -> float
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_preamp(preamp);
        node.set_master(0.0f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };

        double sum_sq = 0.0;
        std::size_t count = 0;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = 0.05f * std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0);
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);

            if (block >= 15)
            {
                for (float sample : output)
                {
                    sum_sq += static_cast<double>(sample) * sample;
                    ++count;
                }
            }
        }
        return static_cast<float>(std::sqrt(sum_sq / static_cast<double>(count)));
    };

    const float low_preamp_rms = measure_rms(0.1f);
    const float high_preamp_rms = measure_rms(1.0f);

    EXPECT_GT(high_preamp_rms, low_preamp_rms);

    // A preamp knob that only moves the output by a hair is not a preamp knob. Full-up must be
    // audibly louder/more driven than nearly-off: at least 6 dB apart.
    EXPECT_GT(high_preamp_rms, low_preamp_rms * 2.0f);
}

TEST(Unit_Dsp, Jcm800OutputTracksInputLevelRatherThanPinningToTheSolverClamp)
{
    // The failure this guards against: with no interstage attenuation the cascaded triodes'
    // grids sit permanently against TriodeStage's +/-60*Vt Newton-Raphson bound, and the amp
    // degenerates into a fixed-amplitude square-wave generator whose output no longer depends on
    // what is played into it. A quiet note and a loud note must still come out at different
    // levels. See the Task 7 report's Newton-Raphson ruling.
    auto measure_peak = [](float input_amplitude) -> float
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_preamp(0.5f);
        node.set_master(0.0f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };

        float peak = 0.0f;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = input_amplitude
                    * static_cast<float>(std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0));
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);

            if (block >= 15)
            {
                for (float sample : output)
                {
                    peak = std::max(peak, std::abs(sample));
                }
            }
        }
        return peak;
    };

    const float quiet_peak = measure_peak(0.05f);
    const float loud_peak = measure_peak(0.2f);

    EXPECT_GT(loud_peak, quiet_peak * 2.0f);
}

TEST(Unit_Dsp, Jcm800DescriptorExposesEightParametersInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.jcm800");
    ASSERT_EQ(descriptor.parameter_count, 8u);

    // The index order is the contract a host and a saved preset address these by, so it is pinned
    // here rather than merely checked for membership.
    const char* expected_ids[8] = {
        "preamp", "master", "bass", "middle", "treble", "presence", "input_trim", "output"
    };
    for (std::size_t i = 0; i < 8; ++i)
    {
        EXPECT_STREQ(descriptor.parameters[i].id, expected_ids[i]);
    }

    EXPECT_EQ(descriptor.parameters[1].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[6].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_EQ(descriptor.parameters[7].kind, SushiDSP::ParamKind::Decibel);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].max, 12.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[1].default_value, 0.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].min, -24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[6].max, 24.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].min, -60.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[7].max, 12.0f);
}

TEST(Unit_Dsp, Jcm800ToneChangeDoesNotRebuildFiltersOffTheAudioThread)
{
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });

    std::vector<float> input(256, 0.0f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };
    SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
    node.process(context);
    ASSERT_FALSE(node.tone_stack_is_dirty());

    // The control-thread call stores a target and nothing else: the coefficients the audio thread
    // last built are still the old ones, which is exactly what "dirty" reports.
    node.set_parameter("bass", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 1.0f);
    EXPECT_TRUE(node.tone_stack_is_dirty());

    // One block is not enough: the knob is smoothed over 20 ms, so the audio thread is still
    // walking the coefficients toward the target. That it converges at all is the audio thread's
    // doing - the control thread never touched a filter.
    node.process(context);
    EXPECT_TRUE(node.tone_stack_is_dirty());

    for (int block = 0; block < 40 && node.tone_stack_is_dirty(); ++block)
    {
        node.process(context);
    }
    EXPECT_FALSE(node.tone_stack_is_dirty());
}

TEST(Unit_Dsp, Jcm800IndexedParameterSetMatchesTheDescriptorOrder)
{
    SushiDSP::Jcm800 node;
    const float values[8] = { 0.2f, -6.0f, 0.4f, 0.45f, 0.6f, 0.7f, 12.0f, -3.0f };
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        node.set_parameter(i, values[i]);
    }

    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
    for (std::uint16_t i = 0; i < 8; ++i)
    {
        EXPECT_FLOAT_EQ(node.get_parameter(descriptor.parameters[i].id), values[i]);
    }

    node.set_parameter(static_cast<std::uint16_t>(99), 1.0f);
}

TEST(Unit_Dsp, Jcm800DecibelParametersClampToTheirDescriptorRange)
{
    SushiDSP::Jcm800 node;
    node.set_master(100.0f);
    node.set_input_trim(-100.0f);
    node.set_output(100.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("master"), 12.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), -24.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), 12.0f);
}

TEST(Unit_Dsp, Jcm800InputTrimAndOutputScaleTheSignalInDecibels)
{
    auto measure_rms = [](float input_trim_db, float output_db) -> double
    {
        SushiDSP::Jcm800 node;
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
        node.set_preamp(0.6f);
        node.set_master(0.0f);
        node.set_input_trim(input_trim_db);
        node.set_output(output_db);

        std::vector<float> input(256);
        std::vector<float> output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { output.data() };

        double sum_sq = 0.0;
        std::size_t count = 0;
        double global_index = 0.0;
        for (int block = 0; block < 30; ++block)
        {
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = 0.02f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 * (global_index + i) / 48000.0));
            }
            global_index += 256.0;

            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);

            if (block >= 20)
            {
                for (float sample : output)
                {
                    sum_sq += static_cast<double>(sample) * sample;
                    ++count;
                }
            }
        }
        return std::sqrt(sum_sq / static_cast<double>(count));
    };

    // The output control is linear and post-cabinet, so -6 dB there is exactly half the level.
    const double reference = measure_rms(0.0f, 0.0f);
    const double halved = measure_rms(0.0f, -6.020599913f);
    EXPECT_NEAR(halved, reference * 0.5, reference * 0.01);

    // The input trim sits ahead of the whole nonlinear chain, so it drives it harder rather than
    // scaling it: at this level the amp is still close to linear, so more in means more out.
    const double trimmed_up = measure_rms(6.0f, 0.0f);
    EXPECT_GT(trimmed_up, reference);
}

TEST(Unit_Dsp, Jcm800DescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();
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

TEST(Unit_Dsp, Jcm800IsDcStableAcrossManyBlocksAtHighPreamp)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(1.0f);
    node.set_master(kUnityMasterDb);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(1.0f);

    std::vector<float> input(256, 0.1f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = { input.data() };
    float* output_channels[1] = { output.data() };

    for (int block = 0; block < 200; ++block)
    {
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        for (float sample : output)
        {
            ASSERT_TRUE(std::isfinite(sample)) << "block " << block;
        }
    }
}

TEST(Unit_Dsp, Jcm800ThdIncreasesWithPreamp)
{
    auto measure_thd_proxy = [](float preamp, double amplitude, int blocks) -> float
    {
        SushiDSP::Jcm800 node;
        SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
        node.prepare(info);
        node.set_preamp(preamp);
        node.set_master(kUnityMasterDb);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.5f);
        node.set_presence(0.5f);

        constexpr float frequency = 220.0f;
        constexpr double sample_rate = 48000.0;
        std::vector<float> output;
        double global_sample_index = 0.0;

        for (int block = 0; block < blocks; ++block)
        {
            std::vector<float> input(256, 0.0f);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                input[i] = static_cast<float>(
                    amplitude * std::sin(2.0 * M_PI * frequency * (global_sample_index + i) / sample_rate));
            }
            global_sample_index += 256.0;

            std::vector<float> block_output(256, 0.0f);
            const float* input_channels[1] = { input.data() };
            float* output_channels[1] = { block_output.data() };
            SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
            node.process(context);
            output.insert(output.end(), block_output.begin(), block_output.end());
        }

        // THD proxy: RMS deviation of the settled output from a best-fit sine at the fundamental,
        // normalized by the output's own RMS. The same measurement D3's Tube Screamer test uses -
        // not a true harmonic decomposition, but monotonic with clipping severity.
        //
        // Only the last quarter of a 120-block run is measured, not D3's tail half of 30 blocks.
        // This chain settles far more slowly: the per-block control slew, the output transformer's
        // 60 Hz highpass, and the power-supply sag stage's 50 ms recovery all still have visible
        // transients well past block 15, and that ramp registers as residual energy large enough
        // to swamp the quantity being measured.
        const std::size_t settle_start = output.size() * 3 / 4;
        double sum_sq = 0.0, sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = settle_start; i < output.size(); ++i)
        {
            const double phase = 2.0 * M_PI * frequency * i / sample_rate;
            sum_sq += static_cast<double>(output[i]) * output[i];
            sum_sin += output[i] * std::sin(phase);
            sum_cos += output[i] * std::cos(phase);
        }
        const std::size_t n = output.size() - settle_start;
        const double fundamental_amplitude = 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
        const double fundamental_power = 0.5 * fundamental_amplitude * fundamental_amplitude * n;
        const double residual_power = std::max(0.0, sum_sq - fundamental_power);
        return static_cast<float>(std::sqrt(residual_power / std::max(1e-12, sum_sq)));
    };

    // Measured at a moderate input level - 0.2, a normal picked note rather than a hot transient -
    // because that is where a gain knob has to earn its name. The knob's job is to sweep the
    // cascade from clean into saturation, so the claim pinned here is the whole upper sweep, not
    // just its endpoints: from the point where V1b's grid first reaches conduction, every step of
    // the knob must add harmonic content.
    const float preamps[] = { 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
    float previous = measure_thd_proxy(0.05f, 0.2, 120);

    // Near the bottom of its travel the amp is genuinely clean at this input level, which is
    // correct behavior for a 2203 and is what makes the rise below meaningful.
    EXPECT_LT(previous, 0.01f);

    for (float preamp : preamps)
    {
        const float thd = measure_thd_proxy(preamp, 0.2, 120);
        EXPECT_GT(thd, previous) << "preamp " << preamp;
        previous = thd;
    }

    // Guards against the monotonic rise above being satisfied by a sequence of indistinguishable
    // near-zero readings: full preamp must produce heavy, obviously audible clipping.
    EXPECT_GT(previous, 0.2f);

    // At a hot input V1a - which sits ahead of the preamp pot and so distorts wherever the knob
    // is - contributes a floor of its own, and the knob's first third mostly rebalances the
    // spectrum rather than adding clipping (measured at a 0.7 input: 0.224 at preamp 0.05, 0.196
    // at 0.3, then rising to 0.340 at 1.0). The upper two thirds still climb, so that is what is
    // asserted here rather than a monotonic law across the whole knob.
    EXPECT_GT(measure_thd_proxy(1.0f, 0.7, 120), measure_thd_proxy(0.4f, 0.7, 120));
}

TEST(Unit_Dsp, Jcm800CabinetStageEngagesAtANonDefaultBlockSize)
{
    // The cabinet's partition is derived from the sample rate, not from PrepareInfo::max_block,
    // so a host running any block size gets the cabinet, not only a 256-frame one. This guards
    // against a regression to a block-size-dependent partition, which used to bypass the cabinet
    // outright whenever the host's block size did not match it.
    constexpr std::uint32_t block_size = 128;
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    auto run = [block_size](std::vector<float> cabinet) -> std::vector<float>
    {
        SushiDSP::Jcm800 node(std::move(cabinet));
        SushiDSP::PrepareInfo info{ 48000.0, block_size, 1 };
        node.prepare(info);
        node.set_preamp(0.6f);
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

TEST(Unit_Dsp, Jcm800PowerTubeBiasMismatchShapesTheEvenHarmonicContent)
{
    // Guards the power-tube bias mismatch (Jcm800.cpp's kPowerTubeGridBiasMismatchVolts) against
    // being dropped or neutralized. It is asserted at the amp's output rather than at the pair
    // because the pair itself is not reachable from here; the mechanism's own proof - that a
    // cathode-bias difference is provably inert and an input-referred offset is not - lives in
    // test_pentode_stage.cpp.
    //
    // The direction is worth stating, because it is not the naive one. The preamp triodes clip
    // one half-cycle harder than the other, so the amp already carries even-harmonic content
    // before the output section; the EL34 pair's handover asymmetry is out of phase with it and
    // partly cancels it. The band below brackets the shipped figure well clear of the
    // no-mismatch one.
    //
    // Re-measured at 220 Hz, 0.2 input, preamp 0.6 after D6a Task 5 moved the tone stack inside
    // the single 8x region and gave the triode grids their Miller input capacitance: second
    // harmonic is 2.17% of the fundamental as shipped and 2.93% with
    // kPowerTubeGridBiasMismatchVolts set to zero. Both figures moved up by about 0.45 pp from
    // the two-region measurement (1.77% / 2.45%) because the drive reaching the power stage is
    // no longer shaped by the old region-A decimator, but the mismatch's effect is unchanged in
    // both sign and size: it still removes about 0.76 pp of second harmonic.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(kUnityMasterDb);
    node.set_bass(0.5f);
    node.set_middle(0.5f);
    node.set_treble(0.5f);
    node.set_presence(0.5f);

    constexpr double sample_rate = 48000.0;
    constexpr double frequency = 220.0;
    std::vector<float> output;
    double global_sample_index = 0.0;
    for (int block = 0; block < 120; ++block)
    {
        std::vector<float> input(256, 0.0f);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            input[i] = static_cast<float>(
                0.2 * std::sin(2.0 * M_PI * frequency * (global_sample_index + i) / sample_rate));
        }
        global_sample_index += 256.0;

        std::vector<float> block_output(256, 0.0f);
        const float* input_channels[1] = { input.data() };
        float* output_channels[1] = { block_output.data() };
        SushiDSP::ProcessContext context{ output_channels, input_channels, 256, 0.0 };
        node.process(context);
        output.insert(output.end(), block_output.begin(), block_output.end());
    }

    // Only the settled last quarter is measured, for the same reason Jcm800ThdIncreasesWithPreamp
    // measures only that: the slew, the transformer highpass and the sag recovery are all still
    // moving well past block 15.
    const std::size_t settle_start = output.size() * 3 / 4;
    const std::size_t n = output.size() - settle_start;
    auto harmonic_amplitude = [&](int harmonic)
    {
        double sum_sin = 0.0, sum_cos = 0.0;
        for (std::size_t i = settle_start; i < output.size(); ++i)
        {
            const double phase = 2.0 * M_PI * frequency * harmonic * i / sample_rate;
            sum_sin += output[i] * std::sin(phase);
            sum_cos += output[i] * std::cos(phase);
        }
        return 2.0 * std::sqrt(sum_sin * sum_sin + sum_cos * sum_cos) / n;
    };

    const double second_harmonic_ratio = harmonic_amplitude(2) / harmonic_amplitude(1);
    std::printf("[ measure ] second harmonic %.4f\n", second_harmonic_ratio);
    EXPECT_GT(second_harmonic_ratio, 0.017);
    EXPECT_LT(second_harmonic_ratio, 0.025);
}

TEST(Unit_Dsp, Jcm800GenericParameterRoundTrip)
{
    SushiDSP::Jcm800 node;
    node.set_parameter("preamp", 0.2f);
    node.set_parameter("master", 0.3f);
    node.set_parameter("bass", 0.4f);
    node.set_parameter("middle", 0.5f);
    node.set_parameter("treble", 0.6f);
    node.set_parameter("presence", 0.7f);
    node.set_parameter("input_trim", 6.0f);
    node.set_parameter("output", -3.0f);

    EXPECT_FLOAT_EQ(node.get_parameter("preamp"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("master"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.7f);
    EXPECT_FLOAT_EQ(node.get_parameter("input_trim"), 6.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("output"), -3.0f);
}

TEST(Unit_Dsp, Jcm800UnknownParameterIsSafeNoOp)
{
    SushiDSP::Jcm800 node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}

TEST(Unit_Dsp, Jcm800CabinetSurvivesVariableBlockSizes)
{
    // StreamingConvolution re-blocks the stream internally, so the cabinet must survive a host
    // that hands the node a different frame count every callback - including counts that are not
    // powers of two and not multiples of the partition. An identity IR makes the whole cabinet
    // stage a pure delay of exactly one partition, so the with-cabinet stream must reproduce the
    // no-cabinet stream shifted by latency_samples()'s reported difference, sample for sample.
    std::vector<float> impulse_response(64, 0.0f);
    impulse_response[0] = 1.0f;

    SushiDSP::Jcm800 with_cabinet(impulse_response);
    SushiDSP::Jcm800 without_cabinet;
    for (SushiDSP::Jcm800* node : { &with_cabinet, &without_cabinet })
    {
        node->prepare(SushiDSP::PrepareInfo{ 48000.0, 512, 1 });
    }

    std::vector<float> input(512), output_with(512, 0.0f), output_without(512, 0.0f);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(0.03f * static_cast<float>(i));
    }
    const float* input_channels[1] = { input.data() };
    float* with_channels[1] = { output_with.data() };
    float* without_channels[1] = { output_without.data() };

    std::vector<float> stream_with, stream_without;
    const std::uint32_t chunks[] = { 100, 37, 512, 1, 300 };
    for (std::uint32_t frames : chunks)
    {
        SushiDSP::ProcessContext context_with{ with_channels, input_channels, frames, 0.0 };
        with_cabinet.process(context_with);
        SushiDSP::ProcessContext context_without{ without_channels, input_channels, frames, 0.0 };
        without_cabinet.process(context_without);
        stream_with.insert(stream_with.end(), output_with.begin(), output_with.begin() + frames);
        stream_without.insert(stream_without.end(), output_without.begin(), output_without.begin() + frames);
    }

    const std::uint32_t latency = with_cabinet.latency_samples() - without_cabinet.latency_samples();
    EXPECT_EQ(latency, 128u);
    for (std::size_t n = latency; n < stream_with.size(); ++n)
    {
        EXPECT_NEAR(stream_with[n], stream_without[n - latency], 1e-5f);
    }
}

TEST(Unit_Dsp, Jcm800NeverExceedsFullScaleWithoutALimiter)
{
    // The soft-knee limiter is gone; a plain clamp is what bounds the output. Driving preamp,
    // master and output to their ceilings must therefore still leave every sample inside [-1, 1],
    // and must actually reach the rail - if nothing ever hits 1.0 the test would be proving only
    // that the amp has headroom, not that the clamp works.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_parameter("preamp", 1.0f);
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
            input[i] = std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * t);
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

TEST(Unit_Dsp, Jcm800RejectsACabinetImpulseResponseAtTheWrongSampleRate)
{
    // StreamingConvolution convolves in samples, not seconds: an IR captured at another rate
    // would play back at the wrong length and the wrong frequency response. Rejecting it keeps
    // the previously active cabinet in place rather than silently installing a detuned one.
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_FALSE(node.has_cabinet());
    const std::uint32_t dry_latency = node.latency_samples();

    SushiDSP::Ir wrong_rate;
    wrong_rate.sample_rate = 44100.0;
    wrong_rate.samples.assign(512, 0.0f);
    wrong_rate.samples[0] = 1.0f;
    EXPECT_FALSE(node.set_cabinet(wrong_rate));
    EXPECT_FALSE(node.has_cabinet());
    EXPECT_EQ(node.latency_samples(), dry_latency);

    SushiDSP::Ir right_rate = wrong_rate;
    right_rate.sample_rate = 48000.0;
    EXPECT_TRUE(node.set_cabinet(right_rate));
    EXPECT_TRUE(node.has_cabinet());
    node.reclaim_cabinets();
}

TEST(Unit_Dsp, Jcm800RejectsACabinetBeforePrepare)
{
    // sample_rate_ defaults to 48000 but the partition is only chosen in prepare(), so the rate
    // check alone would let a 48 kHz IR through and publish a zero-partition convolver whose
    // FIFOs process() would index out of bounds. Rejected outright and not retained: a false
    // return must mean nothing was installed and nothing was armed for later.
    SushiDSP::Jcm800 node;

    SushiDSP::Ir impulse_response;
    impulse_response.sample_rate = 48000.0;
    impulse_response.samples.assign(512, 0.0f);
    impulse_response.samples[0] = 1.0f;

    EXPECT_FALSE(node.set_cabinet(impulse_response));
    EXPECT_FALSE(node.has_cabinet());

    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_FALSE(node.has_cabinet());

    EXPECT_TRUE(node.set_cabinet(impulse_response));
    EXPECT_TRUE(node.has_cabinet());
    node.reclaim_cabinets();
}

TEST(Unit_Dsp, Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput)
{
    // The shipped calibration point: a -12 dBFS low-E sine into an untouched amp comes back out
    // at about -6 dBFS, leaving headroom for a hotter pickup without the post-cabinet clamp ever
    // engaging on a normal signal.
    SushiDSP::Jcm800 node;
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
    EXPECT_GT(peak_decibels, -8.0f);
    EXPECT_LT(peak_decibels, -4.0f);
}

TEST(Unit_Dsp, Jcm800DryLatencyIsOneOversampledRegion)
{
    // D6a.3 collapses V1a..sag into a single 8x region, so the reported latency is one
    // Oversampler8x round trip - not two. A host compensates by this number, so it is pinned
    // against the oversampler itself rather than against a literal.
    SushiDSP::Jcm800 node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 256, 1 });
    EXPECT_FALSE(node.has_cabinet());

    SushiDSP::Oversampler8x reference(256);
    EXPECT_EQ(node.latency_samples(), reference.latency_samples());
}
