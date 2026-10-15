/**************************************************************************/
/* test_mixer_node.cpp                                                    */
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
 * @file test_mixer_node.cpp
 * @brief Tests the summing and per-input gains of MixerNode.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/nodes/routing/splitter_node.hpp>

TEST(Small_MixerNode, MixerNodeSumsTwoInputsWithDefaultUnityGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 64, 1});

    std::vector<float> a(64, 2.0f);
    std::vector<float> b(64, 3.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {a.data(), b.data(), nullptr, nullptr,
                                                              nullptr,  nullptr,  nullptr, nullptr};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 64, 0.0};

    node.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 5.0f);
    }
}

TEST(Small_MixerNode, MixerNodeAppliesPerInputGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 64, 1});
    node.set_parameter_for_control_thread("gain_0", 0.5f);
    node.set_parameter_for_control_thread("gain_1", 2.0f);

    std::vector<float> a(64, 4.0f);
    std::vector<float> b(64, 1.0f);
    std::vector<float> output(64, 0.0f);
    const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {a.data(), b.data(), nullptr, nullptr,
                                                              nullptr,  nullptr,  nullptr, nullptr};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 64, 0.0};

    node.process(context);

    // 4.0*0.5 + 1.0*2.0 = 4.0
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}

TEST(Small_MixerNode, MixerNodeGetParameterRoundTripsEveryGain)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 64, 1});
    node.set_parameter_for_control_thread("gain_3", 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_3"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_0"), 1.0f); // untouched slots default to 1.0
}

TEST(Small_MixerNode, MixerNodeUnknownParameterIdIsSafeNoOp)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 64, 1});
    node.set_parameter_for_control_thread("not_a_real_id", 0.5f); // must not crash
    EXPECT_FLOAT_EQ(node.get_parameter("not_a_real_id"), 0.0f);
}

TEST(Small_MixerNode, MixerNodeDescriptorDeclaresEightGainParameters)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::mixer_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.mixer");
    ASSERT_EQ(descriptor.parameter_count, SushiDSP::MixerNode::K_MAX_FAN_IN);
    EXPECT_STREQ(descriptor.parameters[0].id, "gain_0");
    EXPECT_FLOAT_EQ(descriptor.parameters[0].default_value, 1.0f);
}

TEST(Small_MixerNode, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> a(256, 1.0f);
    std::vector<float> b(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {a.data(), b.data(), nullptr, nullptr,
                                                              nullptr,  nullptr,  nullptr, nullptr};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 100; ++block)
    {
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

namespace
{
    /// @brief Emits a fixed value on its one output channel.
    class ConstantSourceNode : public SushiDSP::AudioNode
    {
        public:
            explicit ConstantSourceNode(float value) : value_(value)
            {

            }

            void prepare(const SushiDSP::PrepareInfo&) override
            {

            }

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    context.outputs[0][frame] = value_;
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            void reset() override
            {

            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override
            {

            }

        private:
            float value_;
    };

    /// @brief Scales its one input by a fixed gain.
    class GainNode : public SushiDSP::AudioNode
    {
        public:
            explicit GainNode(float gain) : gain_(gain)
            {

            }

            void prepare(const SushiDSP::PrepareInfo&) override
            {

            }

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                    context.outputs[0][frame] = context.inputs[0][frame] * gain_;
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            void reset() override
            {

            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override
            {

            }

        private:
            float gain_;
    };
} // namespace

TEST(Small_MixerNode, MixerNodeJoinsTwoBranchesASplitterFedInARigGraph)
{
    SushiDSP::RigGraph graph(256, 6);
    const auto source = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    const auto splitter = graph.add_node(std::make_unique<SushiDSP::SplitterNode>());
    const auto branch_a = graph.add_node(std::make_unique<GainNode>(2.0f));
    const auto branch_b = graph.add_node(std::make_unique<GainNode>(3.0f));
    const auto mixer = graph.add_node(std::make_unique<SushiDSP::MixerNode>());

    graph.connect(source, splitter);
    graph.connect(splitter, branch_a);
    graph.connect(splitter, branch_b);
    graph.connect(branch_a, mixer);
    graph.connect(branch_b, mixer);

    graph.node(mixer)->set_parameter_for_control_thread("gain_0", 0.5f);
    graph.node(mixer)->set_parameter_for_control_thread("gain_1", 1.0f);

    graph.compile(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
    graph.process(context);

    // source(1.0) -split-> branch_a(*2=2.0), branch_b(*3=3.0) -> mixer(2.0*0.5 + 3.0*1.0 = 4.0)
    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 4.0f);
    }
}
