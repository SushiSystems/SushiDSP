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
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/Jcm800.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, Jcm800ProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(0.7f);
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

TEST(Unit_Dsp, Jcm800PortsAreMonoInMonoOut)
{
    SushiDSP::Jcm800 node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, Jcm800MasterZeroProducesSilenceAfterSettling)
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
        node.set_master(0.7f);
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
        node.set_master(0.7f);
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

TEST(Unit_Dsp, Jcm800DescriptorExposesSixControlParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::jcm800_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.jcm800");
    ASSERT_EQ(descriptor.parameter_count, 6u);

    bool has_preamp = false, has_master = false, has_bass = false;
    bool has_middle = false, has_treble = false, has_presence = false;
    for (std::size_t i = 0; i < descriptor.parameter_count; ++i)
    {
        const std::string param_id = descriptor.parameters[i].id;
        if (param_id == "preamp") has_preamp = true;
        if (param_id == "master") has_master = true;
        if (param_id == "bass") has_bass = true;
        if (param_id == "middle") has_middle = true;
        if (param_id == "treble") has_treble = true;
        if (param_id == "presence") has_presence = true;
    }
    EXPECT_TRUE(has_preamp);
    EXPECT_TRUE(has_master);
    EXPECT_TRUE(has_bass);
    EXPECT_TRUE(has_treble);
    EXPECT_TRUE(has_middle);
    EXPECT_TRUE(has_presence);
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
    node.set_master(1.0f);
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
        node.set_master(1.0f);
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
    // The cabinet's partition size is derived from PrepareInfo::max_block, so a host running any
    // power-of-two block size gets the cabinet, not only a 256-frame one. This guards against a
    // regression to a hardcoded partition size, which would silently bypass the cabinet here.
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
        node.set_master(0.7f);

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
    // partly cancels it. Measured at 220 Hz, 0.2 input, preamp 0.6: second harmonic is 1.77% of
    // the fundamental as shipped, and 2.45% with the mismatch zeroed. The band below brackets the
    // shipped figure well clear of the no-mismatch one.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_preamp(0.6f);
    node.set_master(1.0f);
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
    EXPECT_GT(second_harmonic_ratio, 0.012);
    EXPECT_LT(second_harmonic_ratio, 0.021);
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

    EXPECT_FLOAT_EQ(node.get_parameter("preamp"), 0.2f);
    EXPECT_FLOAT_EQ(node.get_parameter("master"), 0.3f);
    EXPECT_FLOAT_EQ(node.get_parameter("bass"), 0.4f);
    EXPECT_FLOAT_EQ(node.get_parameter("middle"), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("treble"), 0.6f);
    EXPECT_FLOAT_EQ(node.get_parameter("presence"), 0.7f);
}

TEST(Unit_Dsp, Jcm800UnknownParameterIsSafeNoOp)
{
    SushiDSP::Jcm800 node;
    node.set_parameter("does_not_exist", 1.0f);
    EXPECT_FLOAT_EQ(node.get_parameter("does_not_exist"), 0.0f);
}
