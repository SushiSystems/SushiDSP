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
    EXPECT_TRUE(controller.chain_ids().empty());
}

TEST(Unit_Dsp, RigControllerAppendAddsNodeToEndOfChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    EXPECT_EQ(ids[1], "sushidsp.jcm800");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerMoveReordersChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.move(0, 1);  // move index 0 to after index 1

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
    EXPECT_EQ(ids[1], "sushidsp.tubescreamer");
}

TEST(Unit_Dsp, RigControllerRemoveDropsFromChain)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.remove(0);

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "sushidsp.jcm800");
}

TEST(Unit_Dsp, RigControllerAppendWithUnknownDescriptorIdIsSafeNoOp)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");

    // An unknown id (e.g. from a corrupt preset file) makes SushiDSP::make_node() return
    // nullptr. rebuild_and_publish() must not crash, and the slot that failed to construct
    // must not linger as a phantom entry in the reported chain.
    controller.append("sushidsp.does_not_exist");

    const auto ids = controller.chain_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], "sushidsp.tubescreamer");
    ASSERT_NE(controller.active_graph(), nullptr);
}

TEST(Unit_Dsp, RigControllerPreservesParameterValuesAcrossReorder)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "drive", 0.42f);

    controller.append("sushidsp.jcm800");
    controller.move(0, 1);

    // tube_screamer is now at index 1 after the move.
    EXPECT_FLOAT_EQ(controller.get_parameter(1, "drive"), 0.42f);
}

TEST(Unit_Dsp, RigControllerGetParameterReturnsDescriptorDefaultBeforeAnySet)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");

    // Find the "drive" parameter's declared default from the registry, independent of
    // set_parameter() ever having been called - a freshly-inserted slot's parameter map
    // must already mirror the node's real defaults (see Task 7 review: insert() used to
    // leave the map empty, so get_parameter() silently returned 0.0f instead of the
    // node's actual default).
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
    ASSERT_NE(expected_default, 0.0f); // guard against a vacuously-true test if the default is ever 0

    EXPECT_FLOAT_EQ(controller.get_parameter(0, "drive"), expected_default);
}

TEST(Unit_Dsp, RigControllerLoadChainSeedsDefaultsForParametersMissingFromPreset)
{
    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    RigController controller(info);

    // A preset that only specifies "drive", omitting every other parameter. Before this fix,
    // load_chain() assigned the incoming map wholesale, so get_parameter() would return 0.0f
    // for them instead of TubeScreamer's declared defaults - and a subsequent save_preset()
    // would then permanently write 0.0 for a parameter that was never actually touched. Level
    // and input trim are in decibels, so their default of 0.0f is the meaningful one here; the
    // non-vacuous coverage comes from "tone".
    std::vector<std::string> ids{ "sushidsp.tubescreamer" };
    std::vector<std::map<std::string, float>> params{ { { "drive", 0.9f } } };

    controller.load_chain(ids, params);

    EXPECT_FLOAT_EQ(controller.get_parameter(0, "drive"), 0.9f);
    EXPECT_FLOAT_EQ(controller.get_parameter(0, "tone"), 0.5f);
    EXPECT_FLOAT_EQ(controller.get_parameter(0, "level"), 0.0f);
    EXPECT_FLOAT_EQ(controller.get_parameter(0, "input_trim"), 0.0f);
}

TEST(Unit_Dsp, RigControllerSetParameterReachesNodeOnlyAfterProcess)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "tone", 0.9f);

    SushiDSP::IAudioNode* node = controller.active_graph()->node(0);
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
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "tone", 0.9f);

    // The controller's own view is authoritative for the GUI and for preset saving, and must
    // not wait for the audio thread to drain the ring.
    EXPECT_FLOAT_EQ(controller.get_parameter(0, "tone"), 0.9f);
}

TEST(Unit_Dsp, RigControllerRebuildSeedsFreshNodesWithStoredParameters)
{
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    RigController controller(info);
    controller.append("sushidsp.tubescreamer");
    controller.set_parameter(0, "tone", 0.9f);
    controller.append("sushidsp.jcm800");

    // The rebuild triggered by the second append constructs a brand-new TubeScreamer, which
    // is seeded off the audio thread before publication - no ring drain needed.
    SushiDSP::IAudioNode* node = controller.active_graph()->node(0);
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->get_parameter("tone"), 0.9f);
}

// --- Rack-list selection remapping (apps/host_gui/SlotSelection.hpp) ---
//
// main.cpp's rack list can insert, move or remove a slot in the middle of an ImGui frame, and
// the parameter panel below it draws whatever `selected_slot` names. No test can drive an ImGui
// frame here, so the index arithmetic lives in a pure header and is pinned directly.

TEST(Unit_Dsp, SlotSelectionRemapAfterInsertShiftsSelectionAtOrAfterTheInsert)
{
    EXPECT_EQ(slot_selection::remap_after_insert(-1, 0), -1);
    EXPECT_EQ(slot_selection::remap_after_insert(0, 0), 1);
    EXPECT_EQ(slot_selection::remap_after_insert(2, 2), 3);
    EXPECT_EQ(slot_selection::remap_after_insert(1, 2), 1);
}

TEST(Unit_Dsp, SlotSelectionRemapAfterRemoveDropsTheRemovedSlotAndShiftsLaterOnes)
{
    EXPECT_EQ(slot_selection::remap_after_remove(1, 1), -1);
    EXPECT_EQ(slot_selection::remap_after_remove(2, 1), 1);
    EXPECT_EQ(slot_selection::remap_after_remove(0, 1), 0);
    EXPECT_EQ(slot_selection::remap_after_remove(-1, 0), -1);
}

TEST(Unit_Dsp, SlotSelectionRemapAfterMoveFollowsTheChainReorder)
{
    // Cross-checked against RigController::move() itself: whatever descriptor id the selection
    // named before the move, it must still name after it.
    SushiDSP::PrepareInfo info{ 48000.0, 64, 1 };
    for (std::size_t from = 0; from < 3; ++from)
    {
        for (std::size_t to = 0; to < 3; ++to)
        {
            for (int selected = 0; selected < 3; ++selected)
            {
                RigController controller(info);
                controller.append("sushidsp.tubescreamer");
                controller.append("sushidsp.jcm800");
                controller.append("sushidsp.tubescreamer");
                // Tag the selected slot so it is identifiable by value, not by descriptor id.
                const auto before = controller.chain_ids();
                controller.set_parameter(static_cast<std::size_t>(selected), "drive", 0.77f);
                const float marker = controller.get_parameter(static_cast<std::size_t>(selected), "drive");

                controller.move(from, to);
                const int remapped = slot_selection::remap_after_move(selected, from, to);

                ASSERT_GE(remapped, 0);
                ASSERT_LT(remapped, 3);
                EXPECT_EQ(controller.chain_ids()[static_cast<std::size_t>(remapped)], before[static_cast<std::size_t>(selected)])
                    << "from " << from << " to " << to << " selected " << selected;
                EXPECT_FLOAT_EQ(controller.get_parameter(static_cast<std::size_t>(remapped), "drive"), marker)
                    << "from " << from << " to " << to << " selected " << selected;
            }
        }
    }
}

TEST(Unit_Dsp, SlotSelectionClampDropsAnOutOfRangeSelection)
{
    EXPECT_EQ(slot_selection::clamp(2, 3u), 2);
    EXPECT_EQ(slot_selection::clamp(3, 3u), -1);
    EXPECT_EQ(slot_selection::clamp(0, 0u), -1);
    EXPECT_EQ(slot_selection::clamp(-1, 3u), -1);
}
