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
