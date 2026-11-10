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
 * @file test_mixer_node.cpp
 * @brief Tests the summing and per-input gains of MixerNode.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/nodes/routing/splitter_node.hpp>

#include "support/in_memory_nodes.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds the frames of one block in the tests below.
    constexpr std::uint32_t K_FRAMES = 64;

    /// @brief Holds the blocks the allocation test processes.
    constexpr int K_TRAP_BLOCKS = 100;

    /// @brief Holds the value the round-trip test writes to each gain.
    constexpr float K_WRITTEN_GAIN = 0.25f;

    /// @brief One gain slot of the mixer.
    struct GainRow
    {
            const char* name;
            const char* id;
    };

    /// @brief Names a gain row by its name.
    std::string gain_row_name(const ::testing::TestParamInfo<GainRow>& info)
    {
        return info.param.name;
    }

    /// @brief Prepares a mixer for blocks of K_FRAMES frames.
    void prepare(SushiDSP::MixerNode& node)
    {
        node.prepare(SushiDSP::PrepareInfo{48000.0, K_FRAMES, 1});
    }

    /// @brief Mixes two constant inputs, @p first on slot 0 and @p second on slot 1, and returns
    /// the output block.
    std::vector<float> mix(SushiDSP::MixerNode& node, float first, float second)
    {
        std::vector<float> a(K_FRAMES, first);
        std::vector<float> b(K_FRAMES, second);
        std::vector<float> output(K_FRAMES, 0.0f);
        const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {
            a.data(), b.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        float* outputs[1] = {output.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_FRAMES, 0.0};
        node.process(context);
        return output;
    }

    /// @brief Processes @p blocks blocks of @p context on @p node.
    void process_blocks(SushiDSP::MixerNode& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
            node.process(context);
    }

    class Small_MixerNodeGain : public ::testing::TestWithParam<GainRow>
    {
    };
} // namespace

TEST(Small_MixerNode, MixerNodeSumsTwoInputsWithDefaultUnityGain)
{
    SushiDSP::MixerNode node;
    prepare(node);

    // 2 + 3 is exact in float.
    EXPECT_EQ(mix(node, 2.0f, 3.0f), std::vector<float>(K_FRAMES, 5.0f));
}

TEST(Small_MixerNode, MixerNodeAppliesPerInputGain)
{
    SushiDSP::MixerNode node;
    prepare(node);
    node.set_parameter_for_control_thread("gain_0", 0.5f);
    node.set_parameter_for_control_thread("gain_1", 2.0f);

    // 4.0 * 0.5 + 1.0 * 2.0 = 4.0, every term exact in float.
    EXPECT_EQ(mix(node, 4.0f, 1.0f), std::vector<float>(K_FRAMES, 4.0f));
}

TEST_P(Small_MixerNodeGain, MixerNodeGetParameterRoundTripsTheGain)
{
    SushiDSP::MixerNode node;
    prepare(node);

    node.set_parameter_for_control_thread(GetParam().id, K_WRITTEN_GAIN);

    EXPECT_FLOAT_EQ(node.get_parameter(GetParam().id), K_WRITTEN_GAIN);
}

INSTANTIATE_TEST_SUITE_P(Slots, Small_MixerNodeGain,
                         ::testing::Values(GainRow{"Gain0", "gain_0"}, GainRow{"Gain1", "gain_1"},
                                           GainRow{"Gain2", "gain_2"}, GainRow{"Gain3", "gain_3"},
                                           GainRow{"Gain4", "gain_4"}, GainRow{"Gain5", "gain_5"},
                                           GainRow{"Gain6", "gain_6"}, GainRow{"Gain7", "gain_7"}),
                         gain_row_name);

TEST(Small_MixerNode, MixerNodeUnknownParameterIdChangesNoGain)
{
    SushiDSP::MixerNode node;
    prepare(node);
    node.set_parameter_for_control_thread("gain_0", 0.25f);
    node.set_parameter_for_control_thread("gain_1", 0.75f);

    node.set_parameter_for_control_thread("not_a_real_id", 0.5f);

    EXPECT_FLOAT_EQ(node.get_parameter("gain_0"), 0.25f);
    EXPECT_FLOAT_EQ(node.get_parameter("gain_1"), 0.75f);
    EXPECT_FLOAT_EQ(node.get_parameter("not_a_real_id"), 0.0f);
}

TEST(Medium_MixerNode, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::MixerNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> a(256, 1.0f);
    std::vector<float> b(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[SushiDSP::MixerNode::K_MAX_FAN_IN] = {
        a.data(), b.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_blocks(node, context, K_TRAP_BLOCKS);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_MixerNode, MixerNodeJoinsTwoBranchesASplitterFedInARigGraph)
{
    SushiDSP::RigGraph graph(256, 6);
    const auto source = graph.add_node(TestSupport::source_node(1.0f));
    const auto splitter = graph.add_node(std::make_unique<SushiDSP::SplitterNode>());
    const auto branch_a = graph.add_node(TestSupport::gain_node(2.0f));
    const auto branch_b = graph.add_node(TestSupport::gain_node(3.0f));
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
    EXPECT_EQ(output_buffer, std::vector<float>(256, 4.0f));
}
