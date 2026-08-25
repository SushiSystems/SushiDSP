/**************************************************************************/
/* test_tube_screamer.cpp                                                 */
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

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Unit_Dsp, TubeScreamerProcessesABlockWithoutCrashingOrProducingNonFiniteOutput)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.8f);

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
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

TEST(Unit_Dsp, TubeScreamerPortsAreMonoInMonoOut)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Unit_Dsp, TubeScreamerLevelZeroProducesSilenceAfterSettling)
{
    SushiDSP::TubeScreamer node;
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    node.prepare(info);
    node.set_drive(0.5f);
    node.set_tone(0.5f);
    node.set_level(0.0f);

    std::vector<float> input(256, 0.2f);
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
        EXPECT_NEAR(sample, 0.0f, 1e-3f);
    }
}

TEST(Unit_Dsp, TubeScreamerDescriptorExposesDriveToneLevel)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();

    EXPECT_STREQ(descriptor.id, "sushidsp.tubescreamer");
    ASSERT_EQ(descriptor.parameter_count, 3u);

    bool has_drive = false, has_tone = false, has_level = false;
    for (std::size_t i = 0; i < descriptor.parameter_count; ++i)
    {
        const std::string param_id = descriptor.parameters[i].id;
        if (param_id == "drive") has_drive = true;
        if (param_id == "tone") has_tone = true;
        if (param_id == "level") has_level = true;
    }
    EXPECT_TRUE(has_drive);
    EXPECT_TRUE(has_tone);
    EXPECT_TRUE(has_level);
}

TEST(Unit_Dsp, TubeScreamerDescriptorMakeProducesAWorkingNode)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::tube_screamer_descriptor();
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };

    std::unique_ptr<SushiDSP::IAudioNode> node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    node->prepare(info);

    EXPECT_EQ(node->ports().input_channels, descriptor.ports.input_channels);
    EXPECT_EQ(node->ports().output_channels, descriptor.ports.output_channels);

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
