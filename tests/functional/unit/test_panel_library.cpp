/**************************************************************************/
/* test_panel_library.cpp                                                 */
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

#include <gtest/gtest.h>

#include "ui/PanelLibrary.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"

TEST(Unit_Dsp, PanelLibraryReturnsAHandAuthoredPanelForAKnownNodeType)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.jcm800");
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->control_count, 8u);
    EXPECT_GT(panel->aspect.x, panel->aspect.y);
}

TEST(Unit_Dsp, PanelLibraryReturnsNullForANodeTypeWithNoEntry)
{
    EXPECT_EQ(ui::panel_for("sushidsp.not_a_real_node"), nullptr);
}

TEST(Unit_Dsp, PanelLibraryTableNamesOnlyParametersTheDescriptorDeclares)
{
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        const ui::PanelDescriptor* panel = ui::panel_for(entry.descriptor->id);
        if (panel == nullptr)
        {
            continue;
        }
        for (std::size_t c = 0; c < panel->control_count; ++c)
        {
            const char* param_id = panel->controls[c].param_id;
            bool found = false;
            for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
            {
                if (std::strcmp(entry.descriptor->parameters[p].id, param_id) == 0)
                {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << entry.descriptor->id << " has no parameter '" << param_id << "'";
        }
    }
}

TEST(Unit_Dsp, PanelLibraryEveryTableEntryNamesARegisteredNodeType)
{
    for (const ui::PanelDescriptor& panel : ui::panel_table())
    {
        bool found = false;
        for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
        {
            if (std::strcmp(entry.descriptor->id, panel.descriptor_id) == 0)
            {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "panel table names unregistered node type '" << panel.descriptor_id << "'";
    }
}

TEST(Unit_Dsp, PanelLibraryAutoPanelPlacesOneControlPerParameterInASquareGrid)
{
    const SushiDSP::NodeDescriptor& jcm = SushiDSP::jcm800_descriptor();
    std::vector<ui::ControlPlacement> storage;
    const ui::PanelDescriptor panel = ui::auto_panel(jcm, storage);

    EXPECT_EQ(panel.control_count, jcm.parameter_count);
    EXPECT_EQ(storage.size(), jcm.parameter_count);

    // 8 parameters fit a 3-column grid, so rows 0, 1 and 2 are used.
    std::size_t distinct_rows = 0;
    float previous_y = -1.0f;
    for (const ui::ControlPlacement& placement : storage)
    {
        if (placement.position.y != previous_y)
        {
            ++distinct_rows;
            previous_y = placement.position.y;
        }
    }
    EXPECT_EQ(distinct_rows, 3u);
}

TEST(Unit_Dsp, PanelLibraryAutoPanelPicksTheControlKindFromTheParameterKind)
{
    constexpr SushiDSP::ParameterDescriptor params[] = {
        { "a", "A", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Linear,  "" },
        { "b", "B", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Boolean, "" },
        { "c", "C", 0.0f, 3.0f, 0.0f, SushiDSP::ParamKind::Choice,  "" },
    };
    const SushiDSP::NodeDescriptor node{ "test.node", "Test", params, 3, { 1, 1 }, nullptr };

    std::vector<ui::ControlPlacement> storage;
    ui::auto_panel(node, storage);

    ASSERT_EQ(storage.size(), 3u);
    EXPECT_EQ(storage[0].kind, ui::ControlKind::Knob);
    EXPECT_EQ(storage[1].kind, ui::ControlKind::Switch);
    EXPECT_EQ(storage[2].kind, ui::ControlKind::Readout);
}

TEST(Unit_Dsp, PanelLibraryMixerShowsOneGainKnobPerConnectedInput)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mixer");
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->is_control_visible, nullptr);

    EXPECT_TRUE(panel->is_control_visible(0, 2));
    EXPECT_TRUE(panel->is_control_visible(1, 2));
    EXPECT_FALSE(panel->is_control_visible(2, 2));
    EXPECT_FALSE(panel->is_control_visible(0, 0));
}

TEST(Unit_Dsp, PanelLibraryLeavesTheVisibilityRuleUnsetForEveryPanelButTheMixer)
{
    for (const ui::PanelDescriptor& panel : ui::panel_table())
    {
        if (std::strcmp(panel.descriptor_id, "sushidsp.mixer") == 0)
        {
            continue;
        }
        EXPECT_EQ(panel.is_control_visible, nullptr) << panel.descriptor_id;
    }
}
