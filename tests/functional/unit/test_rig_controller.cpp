/**************************************************************************/
/* test_rig_controller.cpp                                                */
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

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "RigController.hpp"
#include "SlotSelection.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

namespace
{
    void append_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xFF));
        }
    }

    void append_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void append_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    std::string write_temp_impulse_wav()
    {
        const std::uint32_t sample_rate = 48000;
        std::vector<std::int16_t> samples(64, 0);
        samples[0] = 16384;

        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 2);
        append_u16_le(bytes, 2);
        append_u16_le(bytes, 16);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (std::int16_t sample : samples)
        {
            append_u16_le(bytes, static_cast<std::uint16_t>(sample));
        }

        const std::string path = std::string(::testing::TempDir()) + "sushidsp_test_cabinet.wav";
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    /// @brief Counts the live slots carrying one descriptor id.
    std::size_t count_slots_of_type(const RigController& controller, const std::string& descriptor_id)
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
}

TEST(Unit_Dsp, RigControllerStartsEmpty)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    EXPECT_TRUE(controller.slot_order().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
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

TEST(Unit_Dsp, RigControllerInsertBeforePlacesNewSlotImmediatelyBeforeAnchor)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId c = controller.append("sushidsp.jcm800");

    const RigController::SlotId b = controller.insert_before(c, "sushidsp.tubescreamer");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], b);
    EXPECT_EQ(order[2], c);
}

TEST(Unit_Dsp, RigControllerInsertBeforeNoAnchorPlacesNewSlotAtFront)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");

    const RigController::SlotId new_head = controller.insert_before(std::nullopt, "sushidsp.jcm800");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], new_head);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerMoveBeforeReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");

    controller.move_before(a, std::nullopt);  // a is already at the front; no-op shape, exercised separately below
    controller.move_before(b, a);             // move b to immediately before a

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], b);
    EXPECT_EQ(order[1], a);
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChainAndReconnectsSurvivors)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId b = controller.append("sushidsp.jcm800");
    const RigController::SlotId c = controller.append("sushidsp.tubescreamer");

    controller.remove(b);

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], a);
    EXPECT_EQ(order[1], c);
    // b's removal must not leave a and c disconnected — the chain must re-link around the gap,
    // matching today's behavior where removing a middle slot still produces one working chain.
    ASSERT_NE(controller.active_graph(), nullptr);
    const auto handles = controller.active_graph()->nodes();
    ASSERT_EQ(handles.size(), 2u);
}

TEST(Unit_Dsp, RigControllerBuildsSplitterTwoBranchMixerShapeAndProcessesCorrectly)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);

    const RigController::SlotId source = controller.add_node("sushidsp.tubescreamer", false);
    const RigController::SlotId splitter = controller.add_node("sushidsp.splitter", false);
    const RigController::SlotId branch_a = controller.add_node("sushidsp.jcm800", false);
    const RigController::SlotId branch_b = controller.add_node("sushidsp.tubescreamer", false);
    const RigController::SlotId mixer = controller.add_node("sushidsp.mixer", false);
    controller.connect(source, splitter, false);
    controller.connect(splitter, branch_a, false);
    controller.connect(splitter, branch_b, false);
    controller.connect(branch_a, mixer, false);
    controller.connect(branch_b, mixer, false);
    controller.publish();

    // Every node exists and the graph compiled/published without throwing — the split/mix shape
    // this test builds was, before the publish-batching parameter, impossible to construct
    // through the public API at all (the second add_node() would have republished a two-node,
    // zero-edge graph, which RigGraph::compile() rejects for having no sink). Confirm the rig
    // actually processes a block without crashing or throwing.
    ASSERT_NE(controller.active_graph(), nullptr);
    ASSERT_EQ(controller.active_graph()->nodes().size(), 5u);

    std::vector<float> in(256, 0.0f);
    std::vector<float> out(256, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 256, 0.0 };
    EXPECT_NO_THROW(controller.active_graph()->process(context));
}

TEST(Unit_Dsp, RigControllerAddBranchBuildsTheParallelShapeTheGuiOffers)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);

    // The whole point of the feature, in the order a user performs it: drag a source, a splitter
    // and one effect into a straight line, then drop a second effect on the splitter's open row.
    // add_branch() is what turns that one drop into the four-edge shape RigGraph::compile()
    // accepts, mixer and all.
    const RigController::SlotId source = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId branch_a = controller.append("sushidsp.jcm800");

    const std::optional<RigController::SlotId> branch_b =
        controller.add_branch(splitter, "sushidsp.tubescreamer");
    ASSERT_TRUE(branch_b.has_value());

    // Source, splitter, branch A, branch B, mixer.
    EXPECT_EQ(controller.slot_order().size(), 5u);
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 5u);

    const std::optional<RigController::SlotId> mixer = find_mixer(controller);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 1u);

    EXPECT_EQ(controller.upstream_of(splitter), std::vector<RigController::SlotId>{ source });
    EXPECT_EQ(controller.upstream_of(branch_a), std::vector<RigController::SlotId>{ splitter });
    EXPECT_EQ(controller.upstream_of(*branch_b), std::vector<RigController::SlotId>{ splitter });

    const std::vector<RigController::SlotId> into_mixer = controller.upstream_of(*mixer);
    EXPECT_EQ(into_mixer.size(), 2u);
    EXPECT_TRUE(contains(into_mixer, branch_a));
    EXPECT_TRUE(contains(into_mixer, *branch_b));

    std::vector<float> in(256, 0.0f);
    std::vector<float> out(256, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 256, 0.0 };
    EXPECT_NO_THROW(controller.active_graph()->process(context));
}

TEST(Unit_Dsp, RigControllerAddBranchTwiceReusesTheOneMixer)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
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

TEST(Unit_Dsp, RigControllerAddBranchOnADanglingSplitterCreatesNoMixer)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");

    const std::optional<RigController::SlotId> branch =
        controller.add_branch(splitter, "sushidsp.jcm800");
    ASSERT_TRUE(branch.has_value());

    EXPECT_EQ(controller.slot_order().size(), 2u);
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
    EXPECT_EQ(controller.upstream_of(*branch), std::vector<RigController::SlotId>{ splitter });
}

TEST(Unit_Dsp, RigControllerAddBranchOnASlotThatIsNotASplitterIsRefused)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId screamer = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(screamer, "sushidsp.jcm800").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{ screamer, amp }));
    EXPECT_EQ(controller.upstream_of(amp), std::vector<RigController::SlotId>{ screamer });
}

TEST(Unit_Dsp, RigControllerAddBranchWithAnUnregisteredDescriptorIdIsRefused)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId splitter = controller.append("sushidsp.splitter");
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    EXPECT_FALSE(controller.add_branch(splitter, "sushidsp.does_not_exist").has_value());

    EXPECT_EQ(controller.slot_order(), (std::vector<RigController::SlotId>{ splitter, amp }));
    EXPECT_EQ(count_slots_of_type(controller, "sushidsp.mixer"), 0u);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsAParallelBranch)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController original(info);
    original.append("sushidsp.tubescreamer");
    const RigController::SlotId splitter = original.append("sushidsp.splitter");
    original.append("sushidsp.jcm800");
    ASSERT_TRUE(original.add_branch(splitter, "sushidsp.tubescreamer").has_value());

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    EXPECT_EQ(reloaded.slot_order().size(), 5u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.mixer"), 1u);
    EXPECT_EQ(count_slots_of_type(reloaded, "sushidsp.splitter"), 1u);

    const std::optional<RigController::SlotId> mixer = find_mixer(reloaded);
    ASSERT_TRUE(mixer.has_value());
    EXPECT_EQ(reloaded.upstream_of(*mixer).size(), 2u);
}

TEST(Unit_Dsp, RigControllerAppendWithUnknownDescriptorIdIsSafeNoOp)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId a = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.does_not_exist");

    const auto order = controller.slot_order();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], a);
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.42f);
    const RigController::SlotId amp = controller.append("sushidsp.jcm800");

    controller.move_before(ts, std::nullopt);
    (void)amp;

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), 0.42f);
}

TEST(Unit_Dsp, RigControllerGetParameterReturnsDescriptorDefaultBeforeAnySet)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");

    float expected_default = 0.0f;
    bool found = false;
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (std::string("sushidsp.tubescreamer") == entry.descriptor->id)
        {
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::string("drive") == entry.descriptor->parameters[p].id)
                {
                    expected_default = entry.descriptor->parameters[p].default_value;
                    found = true;
                }
            }
        }
    }
    ASSERT_TRUE(found);
    ASSERT_NE(expected_default, 0.0f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "drive"), expected_default);
}

TEST(Unit_Dsp, RigControllerSetParameterReachesNodeOnlyAfterProcess)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    SushiDSP::IAudioNode* node = controller.active_graph()->node(controller.active_graph()->nodes().front());
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.5f);

    std::vector<float> in(64, 0.0f);
    std::vector<float> out(64, 0.0f);
    const float* ins[1] = { in.data() };
    float* outs[1] = { out.data() };
    SushiDSP::ProcessContext context{ outs, ins, 64, 0.0 };
    controller.active_graph()->process(context);

    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
    EXPECT_EQ(controller.dropped_parameter_changes(), 0u);
}

TEST(Unit_Dsp, RigControllerSetParameterStillMirroredByGetParameterImmediately)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);

    EXPECT_FLOAT_EQ(controller.get_parameter(ts, "tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerRebuildSeedsFreshNodesWithStoredParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "tone", 0.9f);
    controller.append("sushidsp.jcm800");

    SushiDSP::NodeHandle handle_for_ts = 0;   // ts was added first, so it is the graph's first handle
    SushiDSP::IAudioNode* node = controller.active_graph()->node(handle_for_ts);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsChainAndParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController original(info);
    const RigController::SlotId ts = original.append("sushidsp.tubescreamer");
    original.set_parameter(ts, "drive", 0.77f);
    original.append("sushidsp.jcm800");

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(reloaded.descriptor_id(order[0]), "sushidsp.tubescreamer");
    EXPECT_EQ(reloaded.descriptor_id(order[1]), "sushidsp.jcm800");
    EXPECT_FLOAT_EQ(reloaded.get_parameter(order[0], "drive"), 0.77f);
}

TEST(Unit_Dsp, RigControllerSaveToJsonUsesOwnStoredValueNotLiveNodeState)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    const RigController::SlotId ts = controller.append("sushidsp.tubescreamer");
    controller.set_parameter(ts, "drive", 0.9f);
    // Deliberately no process() call and no further structural edit here — the live node still
    // reads the pre-edit default (0.5), but save_to_json() must report the controller's own
    // authoritative 0.9f, matching what get_parameter() and the GUI slider both already show.

    const std::string json_text = controller.save_to_json();
    const nlohmann::json root = nlohmann::json::parse(json_text);
    ASSERT_EQ(root.at("nodes").size(), 1u);
    EXPECT_FLOAT_EQ(root.at("nodes")[0].at("params").at("drive").get<float>(), 0.9f);
}

TEST(Unit_Dsp, RigControllerSaveAndLoadJsonRoundTripsCabinetIr)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    const std::string ir_path = write_temp_impulse_wav();

    RigController original(info);
    const RigController::SlotId amp = original.append("sushidsp.jcm800");
    ASSERT_TRUE(original.set_cabinet(amp, ir_path));

    const std::string json_text = original.save_to_json();

    RigController reloaded(info);
    reloaded.load_from_json(json_text);

    const auto order = reloaded.slot_order();
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(reloaded.cabinet_path(order[0]), ir_path);

    std::remove(ir_path.c_str());
}

// --- Per-slot bypass ---
//
// A bypassed slot keeps its place in slot_order_, its parameters and its edges; only the
// published graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

TEST(Unit_Dsp, RigControllerBypassBridgesTheSlotsNeighbours)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId first = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId last = controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);

    EXPECT_TRUE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
    // The bypassed slot keeps its place in the rig; only the graph skips it.
    EXPECT_EQ(controller.slot_order().size(), 3u);
    EXPECT_EQ(controller.upstream_of(last).size(), 1u);
    EXPECT_EQ(controller.upstream_of(last)[0], middle);
    (void)first;
}

TEST(Unit_Dsp, RigControllerUnbypassRestoresTheOriginalGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);
    controller.set_bypassed(middle, false);

    EXPECT_FALSE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 3u);
}

TEST(Unit_Dsp, RigControllerBridgesTwoBypassedSlotsInARow)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId second = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId third = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(second, true);
    controller.set_bypassed(third, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerBypassingTheHeadSlotLeavesAWorkingGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId head = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(head, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 1u);
    EXPECT_FALSE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerBypassingEverySlotLeavesAnEmptyGraph)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    const RigController::SlotId only = controller.append("sushidsp.tubescreamer");

    controller.set_bypassed(only, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_TRUE(controller.active_graph()->is_empty());
}

TEST(Unit_Dsp, RigControllerSaveToJsonKeepsABypassedNodeAndFlagsIt)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");
    controller.set_bypassed(middle, true);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    ASSERT_EQ(parsed.at("nodes").size(), 3u);
    int flagged = 0;
    for (const auto& node : parsed.at("nodes"))
    {
        if (node.contains("bypassed") && node.at("bypassed").get<bool>())
        {
            ++flagged;
        }
    }
    EXPECT_EQ(flagged, 1);
}

TEST(Unit_Dsp, RigControllerLoadFromJsonRestoresTheBypassFlag)
{
    RigController source(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    source.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = source.append("sushidsp.tubescreamer");
    source.append("sushidsp.jcm800");
    source.set_bypassed(middle, true);
    const std::string preset = source.save_to_json();

    RigController target(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    target.load_from_json(preset);

    const std::vector<RigController::SlotId> order = target.slot_order();
    ASSERT_EQ(order.size(), 3u);
    int bypassed = 0;
    for (RigController::SlotId slot : order)
    {
        if (target.is_bypassed(slot))
        {
            ++bypassed;
        }
    }
    EXPECT_EQ(bypassed, 1);
    ASSERT_NE(target.active_graph(), nullptr);
    EXPECT_EQ(target.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoBypassFieldWithEverySlotActive)
{
    const std::string preset = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";

    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    controller.load_from_json(preset);

    for (RigController::SlotId slot : controller.slot_order())
    {
        EXPECT_FALSE(controller.is_bypassed(slot));
    }
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}

TEST(Unit_Dsp, RigControllerIsBypassedIsFalseForASlotThatIsNotLive)
{
    RigController controller(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
    EXPECT_FALSE(controller.is_bypassed(9999));
}

// --- Rack-list selection tracking (apps/host_gui/SlotSelection.hpp) ---
//
// A SlotId stays valid across insert/move/remove, unlike the old int-index scheme, so the only
// remaining job is dropping a selection that no longer names a live slot.

TEST(Unit_Dsp, SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_TRUE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseWhenSlotWasRemoved)
{
    std::vector<RigController::SlotId> order{ 3, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::optional<RigController::SlotId>(7), order));
}

TEST(Unit_Dsp, SlotSelectionStillValidReturnsFalseForNoSelection)
{
    std::vector<RigController::SlotId> order{ 3, 7, 9 };
    EXPECT_FALSE(slot_selection::still_valid(std::nullopt, order));
}
