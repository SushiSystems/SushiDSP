/**************************************************************************/
/* test_bypass.cpp                                                        */
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
 * @file test_bypass.cpp
 * @brief Tests the per-slot bypass of RigController and how a preset keeps it.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rig_controller.hpp"
#include "support/rig_products.hpp"

namespace
{
    /// @brief Holds the rigs the bypass tests build, on the catalogue of the fixture.
    class Small_RigControllerBypass : public TestSupport::RigProducts
    {
    };

    /// @brief Counts the slots of a rig that are bypassed.
    std::size_t count_bypassed(const RigController& controller)
    {
        std::size_t total = 0;
        for (RigController::SlotId slot : controller.slot_order())
        {
            if (controller.is_bypassed(slot))
            {
                ++total;
            }
        }
        return total;
    }

    /// @brief Counts the nodes of a preset document that carry the bypass flag.
    std::size_t count_flagged_nodes(const nlohmann::json& document)
    {
        std::size_t total = 0;
        for (const auto& node : document.at("nodes"))
        {
            if (node.contains("bypassed") && node.at("bypassed").get<bool>())
            {
                ++total;
            }
        }
        return total;
    }

    /// @brief Builds a screamer, a second screamer and an amplifier in a chain.
    /// @return The middle slot.
    RigController::SlotId build_three_slot_chain(RigController& controller)
    {
        controller.append("sushidsp.tubescreamer");
        const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
        controller.append("sushidsp.jcm800");
        return middle;
    }

    /// @brief Fails unless the published graph feeds its node number @p downstream from its node
    /// number @p upstream and from no other, both counted in the order the graph lists its nodes.
    void expect_published_edge(RigController& controller, std::size_t upstream,
                               std::size_t downstream)
    {
        const auto handles = controller.active_graph()->nodes();
        ASSERT_GT(handles.size(), std::max(upstream, downstream));
        const auto fed_from = controller.active_graph()->upstream_of(handles[downstream]);
        ASSERT_EQ(fed_from.size(), 1u);
        EXPECT_EQ(fed_from[0], handles[upstream]);
    }

    const char* const K_PRESET_WITHOUT_BYPASS = R"({"nodes":[
        {"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]},
        {"id":1,"type":"sushidsp.jcm800","params":{},"inputs":[0]}
    ]})";
} // namespace

TEST_F(Small_RigControllerBypass, BypassKeepsTheSlotInPlaceAndItsEdgesWhileTheGraphSkipsIt)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId middle = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId last = controller.append("sushidsp.jcm800");

    controller.set_bypassed(middle, true);

    EXPECT_TRUE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
    EXPECT_EQ(controller.slot_order().size(), 3u);
    EXPECT_EQ(controller.upstream_of(last), std::vector<RigController::SlotId>{middle});
}

TEST_F(Small_RigControllerBypass, BypassBridgesTheSlotsNeighboursInThePublishedGraph)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.set_bypassed(build_three_slot_chain(controller), true);

    ASSERT_EQ(controller.active_graph()->nodes().size(), 2u);
    expect_published_edge(controller, 0, 1);
}

TEST_F(Small_RigControllerBypass, UnbypassRestoresTheOriginalGraph)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId middle = build_three_slot_chain(controller);

    controller.set_bypassed(middle, true);
    controller.set_bypassed(middle, false);

    EXPECT_FALSE(controller.is_bypassed(middle));
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 3u);
}

TEST_F(Small_RigControllerBypass, TwoBypassedSlotsInARowAreBothLeftOutOfTheGraph)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.append("sushidsp.tubescreamer");
    const RigController::SlotId second = controller.append("sushidsp.tubescreamer");
    const RigController::SlotId third = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(second, true);
    controller.set_bypassed(third, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
    expect_published_edge(controller, 0, 1);
}

TEST_F(Small_RigControllerBypass, BypassingTheHeadSlotLeavesTheAmplifierAloneAsTheRoot)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId head = controller.append("sushidsp.tubescreamer");
    controller.append("sushidsp.jcm800");

    controller.set_bypassed(head, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    ASSERT_EQ(controller.active_graph()->nodes().size(), 1u);
    EXPECT_FALSE(controller.active_graph()->is_empty());
    EXPECT_TRUE(controller.active_graph()
                    ->upstream_of(controller.active_graph()->nodes().front())
                    .empty());
}

TEST_F(Small_RigControllerBypass, BypassingEverySlotLeavesAnEmptyGraph)
{
    RigController controller = rig({48000.0, 64, 1});
    const RigController::SlotId only = controller.append("sushidsp.tubescreamer");

    controller.set_bypassed(only, true);

    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_TRUE(controller.active_graph()->is_empty());
}

TEST_F(Small_RigControllerBypass, SaveToJsonKeepsABypassedNodeAndFlagsIt)
{
    RigController controller = rig({48000.0, 64, 1});
    controller.set_bypassed(build_three_slot_chain(controller), true);

    const nlohmann::json parsed = nlohmann::json::parse(controller.save_to_json());

    EXPECT_EQ(parsed.at("nodes").size(), 3u);
    EXPECT_EQ(count_flagged_nodes(parsed), 1u);
}

TEST_F(Small_RigControllerBypass, LoadFromJsonRestoresTheBypassFlag)
{
    RigController source = rig({48000.0, 64, 1});
    source.set_bypassed(build_three_slot_chain(source), true);
    const std::string preset = source.save_to_json();

    RigController target = rig({48000.0, 64, 1});
    target.load_from_json(preset);

    EXPECT_EQ(target.slot_order().size(), 3u);
    EXPECT_EQ(count_bypassed(target), 1u);
    ASSERT_NE(target.active_graph(), nullptr);
    EXPECT_EQ(target.active_graph()->nodes().size(), 2u);
}

TEST_F(Small_RigControllerBypass, LoadsAPresetWithNoBypassFieldWithEverySlotActive)
{
    RigController controller = rig({48000.0, 64, 1});

    controller.load_from_json(K_PRESET_WITHOUT_BYPASS);

    EXPECT_EQ(count_bypassed(controller), 0u);
    ASSERT_NE(controller.active_graph(), nullptr);
    EXPECT_EQ(controller.active_graph()->nodes().size(), 2u);
}
