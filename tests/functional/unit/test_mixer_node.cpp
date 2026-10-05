/****************************************************************/
/* test_mixer_node.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_mixer_node.cpp
 * @brief Tests the summing and per-input gains of MixerNode.
 * @author Mustafa Garip
 */

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/routing/MixerNode.hpp"

TEST(Unit_Dsp, MixerNodeSumsTwoInputsWithDefaultUnityGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });

    std::vector<float> a(64, 2.0f);
    std::vector<float> b(64, 3.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[SushiDSP::MixerNode::kMaxFanIn] = { a.data(), b.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}

TEST(Unit_Dsp, MixerNodeAppliesPerInputGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter_for_control_thread("gain_0", 0.5f);
    node.set_parameter_for_control_thread("gain_1", 2.0f);

    std::vector<float> a(64, 4.0f);
    std::vector<float> b(64, 1.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[SushiDSP::MixerNode::kMaxFanIn] = { a.data(), b.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    float* outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

    node.process(context);

    // 4.0*0.5 + 1.0*2.0 = 4.0
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}

TEST(Unit_Dsp, MixerNodeGetParameterRoundTripsEveryGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter_for_control_thread("gain_3", 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_3"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_0"), 1.0f);   // untouched slots default to 1.0
}

TEST(Unit_Dsp, MixerNodeUnknownParameterIdIsSafeNoOp)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    node.set_parameter_for_control_thread("not_a_real_id", 0.5f);   // must not crash
    EXPECT_FLOAT_EQ(node.get_parameter("not_a_real_id"), 0.0f);
}

TEST(Unit_Dsp, MixerNodeDescriptorDeclaresEightGainParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mixer_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.mixer");
    ASSERT_EQ(descriptor.parameter_count, SushiDSP::MixerNode::kMaxFanIn);
    EXPECT_STREQ(descriptor.parameters[0].id, "gain_0");
    EXPECT_FLOAT_EQ(descriptor.parameters[0].default_value, 1.0f);
}
