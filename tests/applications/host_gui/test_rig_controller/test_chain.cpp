/**************************************************************************/
/* test_chain.cpp                                                         */
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
 * @file test_chain.cpp
 * @brief Tests how RigController edits its chain, its branches, its parameters and its edges.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "rig_controller.hpp"
#include "support/rig_products.hpp"
#include "support/wav_fixture.hpp"
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>
#include <SushiDSP/session/rig_preset/node_catalogue.hpp>

namespace
{
    /// @brief Holds the rigs the chain tests build, on the catalogue of the fixture.
    class Small_RigControllerChain : public TestSupport::RigProducts
    {
    };

    /// @brief Counts the live slots carrying one descriptor id.
    std::size_t count_slots_of_type(const RigController& controller,
                                    const std::string& descriptor_id)
    {
        std::size_t total = 0;
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == descriptor_id)
            {
                ++total;
            }
        }
        return total;
    }

    /// @brief Finds the rig's first mixer.
    /// @return The mixer's slot id, or std::nullopt when the rig has none.
    std::optional<RigController::SlotId> find_mixer(const RigController& controller)
    {
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.descriptor_id(slot) == "sushidsp.mixer")
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    /// @brief Reports whether a slot id appears in a list.
    bool contains(const std::vector<RigController::SlotId>& slots, RigController::SlotId slot)
    {
        return std::find(slots.begin(), slots.end(), slot) != slots.end();
    }

    /// @brief Runs the rig's published graph over one block of silence.
    /// @param controller The rig to run.
    /// @param frames The block length, at most the rig's prepared block size.
    void process_silent_block(RigController& controller, std::uint32_t frames)
    {
        std::vector<float> in(frames, 0.0f);
        std::vector<float> out(frames, 0.0f);
        const float* ins[1] = {in.data()};
        float* outs[1] = {out.data()};
        SushiDSP::ProcessContext context{outs, ins, frames, 0.0};
        controller.active_graph()->process(context);
    }
    /// @brief Runs one block through the rig's active graph.
    /// @param controller The rig to run.
    /// @param in The block, at most the rig's prepared block size.
    /// @return The block the graph wrote.
    std::vector<float> process_block(RigController& controller, const std::vector<float>& in)
    {
        std::vector<float> out(in.size(), 0.0f);
        const float* ins[1] = {in.data()};
        float* outs[1] = {out.data()};
        SushiDSP::ProcessContext context{outs, ins, static_cast<std::uint32_t>(in.size()), 0.0};
        controller.active_graph()->process(context);
        return out;
    }
} // namespace

TEST_F(Small_RigControllerChain, AppendAddsNodeToEndOfChain)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(controller.descriptor_id(a), "sushidsp.tubescreamer");
    EXPECT_EQ(controller.descriptor_id(b), "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST_F(Small_RigControllerChain, InsertBeforePlacesNewSlotImmediatelyBeforeAnchor)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId c = controller.append("sushidsp.jcm800");

    const RigController::SlotId b = controller.insert_before(c, "sushidsp.tubescreamer");

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{a, b, c}));
}

TEST_F(Small_RigControllerChain, InsertBeforeNoAnchorPlacesNewSlotAtFront)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");

    const RigController::SlotId new_head =
        controller.insert_before(std::nullopt, "sushidsp.jcm800");

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{new_head, a}));
}

TEST_F(Small_RigControllerChain, MoveBeforePlacesTheMovedSlotImmediatelyBeforeTheAnchor)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    controller.move_before(b, a);

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{b, a}));
}

TEST_F(Small_RigControllerChain, RemoveDropsFromChainAndReconnectsSurvivors)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    controller.remove(b);

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{a, c}));
    EXPECT_EQ(controller.upstream_of(c), std::vector<RigController::SlotId>{a});
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

// The splitters copy and the mixer sums at gain 1 each, so the sink holds exactly 2x; see the README.
TEST_F(Small_RigControllerChain, BatchedEditsBuildASplitterTwoBranchMixerShapeThatDoublesABlock)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId source = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId splitter = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId branch_a = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId branch_b = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId mixer = controller.add_node("sushidsp.mixer", false);
    controller.connect(source, splitter, false);
    controller.connect(splitter, branch_a, false);
    controller.connect(splitter, branch_b, false);
    controller.connect(branch_a, mixer, false);
    controller.connect(branch_b, mixer, false);
    controller.publish();
    ASSERT_NE(controller.active_graph(), nullptr);
    ASSERT_EQ(controller.active_graph()->nodes().size(), 5u);
    const std::vector<float> in = TestSupport::make_test_signal(256);

    const std::vector<float> out = process_block(controller, in);

    std::vector<float> doubled(in.size());
    std::transform(in.begin(), in.end(), doubled.begin(),
                   [](float sample)
                   {
                       return 2.0f * sample;
                   });
    TestSupport::expect_same_signal(out, doubled);
}

TEST_F(Small_RigControllerChain, AddBranchBuildsTheParallelShapeTheGuiOffers)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId source = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId branch_a = controller.append("sushidsp.jcm800");

    const std::optional<RigController::SlotId> branch_b =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    ASSERT_TRUE(branch_b.has_value());

    EXPECT_EQ(controller.slot_order().size(), 5u);
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 5u);
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 1u);
    const std::optional<RigController::SlotId> mixer = find_mixer(controller);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(controller.upstream_of(splitter), std::vector<RigController::SlotId>{source});
    EXPECT_EQ(controller.upstream_of(branch_a), std::vector<RigController::SlotId>{splitter});
    EXPECT_EQ(controller.upstream_of(*branch_b), std::vector<RigController::SlotId>{splitter});
    const std::vector<RigController::SlotId> into_mixer = controller.upstream_of(*mixer);
    EXPECT_EQ(into_mixer.size(), 2u);
    EXPECT_TRUE(contains(into_mixer, branch_a));
    EXPECT_TRUE(contains(into_mixer, *branch_b));
}

TEST_F(Small_RigControllerChain, AddBranchLeavesAGraphThatProcessesABlock)
{
    RigController controller = rig({48000.0, 256, 1});
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    controller.append("sushidsp.jcm800");
    ASSERT_TRUE(controller.add_branch(splitter, "sushidsp.tubescreamer").has_value());

    EXPECT_NO_THROW(process_silent_block(controller, 256));
}

TEST_F(Small_RigControllerChain, AddBranchTwiceReusesTheOneMixer)
{
    RigController controller = rig({48000.0, 256, 1});
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId branch_a = controller.append("sushidsp.jcm800");

    const std::optional<RigController::SlotId> branch_b =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    const std::optional<RigController::SlotId> branch_c =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    ASSERT_TRUE(branch_b.has_value());
    ASSERT_TRUE(branch_c.has_value());

    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 1u);
    EXPECT_EQ(controller.slot_order().size(), 6u);
    const std::optional<RigController::SlotId> mixer = find_mixer(controller);
    ASSERT_TRUE(mixer.has_value());
    const std::vector<RigController::SlotId> into_mixer = controller.upstream_of(*mixer);
    EXPECT_EQ(into_mixer.size(), 3u);
    EXPECT_TRUE(contains(into_mixer, branch_a));
    EXPECT_TRUE(contains(into_mixer, *branch_b));
    EXPECT_TRUE(contains(into_mixer, *branch_c));
}

TEST_F(Small_RigControllerChain, AddBranchOnADanglingSplitterCreatesNoMixer)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");

    const std::optional<RigController::SlotId> branch =
        controller.add_branch(splitter, "sushidsp.jcm800");
    ASSERT_TRUE(branch.has_value());

    EXPECT_EQ(controller.slot_order().size(), 2u);
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
    EXPECT_EQ(controller.upstream_of(*branch), std::vector<RigController::SlotId>{splitter});
}

TEST_F(Small_RigControllerChain, AddBranchOnASlotThatIsNotASplitterIsRefused)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(screamer, "sushidsp.jcm800").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{screamer, amp}));
    EXPECT_EQ(controller.upstream_of(amp), std::vector<RigController::SlotId>{screamer});
}

TEST_F(Small_RigControllerChain, AddBranchWithAnUnregisteredDescriptorIdIsRefused)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(splitter, "sushidsp.does_not_exist").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{splitter, amp}));
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
}

TEST_F(Small_RigControllerChain, AppendWithUnknownDescriptorIdKeepsTheSlotAndBuildsNoNodeForIt)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId unknown = controller.append("sushidsp.does_not_exist");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    // The slot keeps its place and its descriptor id; only the graph leaves it out.
    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{a, unknown, b}));
    EXPECT_EQ(controller.descriptor_id(unknown), "sushidsp.does_not_exist");
    ASSERT_NE(controller.active_graph(), nullptr);
    SushiDSP::RigGraph& graph = *controller.active_graph();
    ASSERT_EQ(graph.nodes().size(), 2u);

    // The block passes through: the node after the unknown slot is fed by the one before it.
    std::size_t fed_nodes = 0;
    for (SushiDSP::NodeHandle handle : graph.nodes())
    {
        fed_nodes += graph.upstream_of(handle).size();
    }
    EXPECT_EQ(fed_nodes, 1u);
}

TEST_F(Small_RigControllerChain, AppendBuildsNoNodeForAProductTheInjectedCatalogueDoesNotList)
{
    SushiDSP::InMemoryNodeCatalogue only_the_screamer;
    only_the_screamer.add(SushiDSP::tube_screamer_descriptor());
    RigController controller({48000.0, 256, 1}, RigController::ClockSource{}, only_the_screamer);
    controller.append("sushidsp.tubescreamer");

    controller.append("sushidsp.jcm800");

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 1u);
}

TEST_F(Small_RigControllerChain, ParameterValuesSurviveAReorder)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.42f);

    controller.move_before(ts, std::nullopt);

    ASSERT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{ts, amp}));
    EXPECT_EQ(controller.get_parameter(ts, "drive"), 0.42f);
}

TEST_F(Small_RigControllerChain, GetParameterReturnsTheDescriptorDefaultBeforeAnySet)
{
    RigController controller = rig({48000.0, 256, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    EXPECT_EQ(controller.get_parameter(ts, "drive"), default_of("sushidsp.tubescreamer", "drive"));
}

TEST_F(Small_RigControllerChain, SetParameterLeavesTheNodeAtItsDefaultBeforeProcess)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    controller.set_parameter(ts, "tone", 0.9f);

    SushiDSP::AudioNode* node =
        controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_parameter("tone"), default_of("sushidsp.tubescreamer", "tone"));
}

TEST_F(Small_RigControllerChain, SetParameterReachesTheNodeAfterProcess)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    process_silent_block(controller, 64);

    SushiDSP::AudioNode* node =
        controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_parameter("tone"), 0.9f);
    EXPECT_EQ(controller.dropped_parameter_changes(), 0u);
}

TEST_F(Small_RigControllerChain, SetParameterIsMirroredByGetParameterImmediately)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    controller.set_parameter(ts, "tone", 0.9f);

    EXPECT_EQ(controller.get_parameter(ts, "tone"), 0.9f);
}

TEST_F(Small_RigControllerChain, RebuildSeedsFreshNodesWithStoredParameters)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    controller.append("sushidsp.jcm800");

    SushiDSP::AudioNode* node =
        controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_parameter("tone"), 0.9f);
}

TEST_F(Small_RigControllerChain, EdgesListsStoredEdgesInOrder)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    using Edge = std::pair<RigController::SlotId, RigController::SlotId>;
    EXPECT_EQ(controller.edges(), (std::vector<Edge>{{a, b}, {b, c}}));
}

TEST_F(Small_RigControllerChain, ClearEdgesLeavesSlotsLive)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");
    ASSERT_EQ(controller.edges().size(), 1u);

    controller.clear_edges(false);

    EXPECT_TRUE(controller.edges().empty());
    EXPECT_EQ(controller.slot_order().size(), 2u);
}
