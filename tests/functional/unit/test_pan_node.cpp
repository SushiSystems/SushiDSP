/**************************************************************************/
/* test_pan_node.cpp                                                      */
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

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/routing/PanNode.hpp"

namespace
{
    /// @brief Runs a unit input through node at a fixed pan until the smoother settles, then
    /// returns the resulting left/right gains (the output value, since the input is 1.0).
    void settled_gains(SushiDSP::PanNode& node, float pan, float& left, float& right)
    {
        node.set_parameter_for_control_thread("pan", pan);

        constexpr int kMaxBlocks = 10000;
        std::vector<float> input(64, 1.0f);
        std::vector<float> out_left(64, 0.0f);
        std::vector<float> out_right(64, 0.0f);
        const float* inputs[1] = { input.data() };
        float* outputs[2] = { out_left.data(), out_right.data() };
        SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

        float previous_left = 0.0f;
        float previous_right = 0.0f;
        for (int block = 0; block < kMaxBlocks; ++block)
        {
            node.process(context);
            left = out_left.back();
            right = out_right.back();
            if (block > 0 && left == previous_left && right == previous_right)
            {
                break;
            }
            previous_left = left;
            previous_right = right;
        }
    }

    float to_db(float gain)
    {
        return 20.0f * std::log10(std::fabs(gain));
    }
}

TEST(Unit_Dsp, PanNodeCentreIsMinus3Point01DbOnBothChannels)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    float left = 0.0f, right = 0.0f;
    settled_gains(node, 0.0f, left, right);

    EXPECT_NEAR(to_db(left), -3.01f, 0.01f);
    EXPECT_NEAR(to_db(right), -3.01f, 0.01f);
}

TEST(Unit_Dsp, PanNodeHardLeftIsUnityLeftAndDeepNullRight)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    float left = 0.0f, right = 0.0f;
    settled_gains(node, -1.0f, left, right);

    EXPECT_NEAR(left, 1.0f, 1e-4f);
    EXPECT_LE(to_db(right), -80.0f);
}

TEST(Unit_Dsp, PanNodeHardRightIsUnityRightAndDeepNullLeft)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    float left = 0.0f, right = 0.0f;
    settled_gains(node, 1.0f, left, right);

    EXPECT_NEAR(right, 1.0f, 1e-4f);
    EXPECT_LE(to_db(left), -80.0f);
}

TEST(Unit_Dsp, PanNodeSumOfSquaredGainsStaysUnityAcrossItsRange)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    for (int step = 0; step <= 20; ++step)
    {
        const float pan = -1.0f + step * (2.0f / 20.0f);
        float left = 0.0f, right = 0.0f;
        settled_gains(node, pan, left, right);

        const float power = left * left + right * right;
        EXPECT_NEAR(power, 1.0f, 0.01f) << "pan = " << pan;
    }
}

TEST(Unit_Dsp, PanNodeDescriptorDeclaresOnePanParameterInContractOrder)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::pan_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.pan");
    EXPECT_STREQ(descriptor.name, "Pan");
    ASSERT_EQ(descriptor.parameter_count, 1u);
    EXPECT_STREQ(descriptor.parameters[0].id, "pan");
    EXPECT_FLOAT_EQ(descriptor.parameters[0].min, -1.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[0].max, 1.0f);
    EXPECT_FLOAT_EQ(descriptor.parameters[0].default_value, 0.0f);
}

TEST(Unit_Dsp, PanNodeIndexedSetterAgreesWithNamedSetter)
{
    SushiDSP::PanNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    node.set_parameter_for_audio_thread(static_cast<std::uint16_t>(0), 0.5f);
    EXPECT_FLOAT_EQ(node.get_parameter("pan"), 0.5f);

    node.set_parameter_for_control_thread("pan", -0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("pan"), -0.25f);
}

TEST(Unit_Dsp, PanNodePortsAreOneInputTwoOutputs)
{
    SushiDSP::PanNode node;
    const SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 2u);
}
