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

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "RigController.hpp"
#include "SlotSelection.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

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
