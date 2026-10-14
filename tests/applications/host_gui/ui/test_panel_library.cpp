/**************************************************************************/
/* test_panel_library.cpp                                                 */
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
 * @file test_panel_library.cpp
 * @brief Tests that PanelLibrary returns a panel for every registered node.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/panel_library.hpp"

#include <cstring>
#include <string>
#include <vector>

#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

TEST(PanelLibrary, PanelLibraryReturnsAHandAuthoredPanelForAKnownNodeType)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.jcm800");
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->control_count, 8u);
    EXPECT_GT(panel->aspect.x, panel->aspect.y);
}

TEST(PanelLibrary, PanelLibraryReturnsAHandAuthoredPanelForEveryAmplifier)
{
    std::size_t amplifiers = 0;
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        if (std::strcmp(entry.category.name, "Amp") != 0)
        {
            continue;
        }
        ++amplifiers;
        const ui::PanelDescriptor* panel = ui::panel_for(entry.descriptor->id);
        ASSERT_NE(panel, nullptr) << entry.descriptor->id;
        EXPECT_GT(panel->control_count, 0u) << entry.descriptor->id;
        EXPECT_GT(panel->aspect.x, panel->aspect.y) << entry.descriptor->id;
    }

    // The registry says which products are amplifiers. See tests/applications/host_gui/ui/README.md.
    EXPECT_GT(amplifiers, 0u);
}

TEST(PanelLibrary, PanelLibraryDrawsTheMarkTwoCPlusEqualizerBandsAsFaders)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mesa_mark2c");
    ASSERT_NE(panel, nullptr);

    std::vector<std::string> fader_ids;
    for (std::size_t c = 0; c < panel->control_count; ++c)
    {
        if (panel->controls[c].kind == ui::ControlKind::Slider)
        {
            fader_ids.push_back(panel->controls[c].param_id);
        }
    }

    const std::vector<std::string> expected{"eq_80", "eq_240", "eq_750", "eq_2200", "eq_6600"};
    EXPECT_EQ(fader_ids, expected);
}

TEST(PanelLibrary, PanelLibraryReturnsNullForANodeTypeWithNoEntry)
{
    EXPECT_EQ(ui::panel_for("sushidsp.not_a_real_node"), nullptr);
}

TEST(PanelLibrary, PanelLibraryTableNamesOnlyParametersTheDescriptorDeclares)
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

TEST(PanelLibrary, PanelLibraryTablePlacesAControlForEveryParameterTheDescriptorDeclares)
{
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        const ui::PanelDescriptor* panel = ui::panel_for(entry.descriptor->id);
        if (panel == nullptr)
        {
            continue;
        }
        for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
        {
            const char* param_id = entry.descriptor->parameters[p].id;
            bool placed = false;
            for (std::size_t c = 0; c < panel->control_count; ++c)
            {
                if (std::strcmp(panel->controls[c].param_id, param_id) == 0)
                {
                    placed = true;
                    break;
                }
            }
            EXPECT_TRUE(placed) << entry.descriptor->id << " has no control for parameter '"
                                << param_id << "'";
        }
    }
}

TEST(PanelLibrary, PanelLibraryEveryTableEntryNamesARegisteredNodeType)
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
        EXPECT_TRUE(found) << "panel table names unregistered node type '" << panel.descriptor_id
                           << "'";
    }
}

TEST(PanelLibrary, PanelLibraryAutoPanelPlacesOneControlPerParameterInASquareGrid)
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

TEST(PanelLibrary, PanelLibraryAutoPanelPicksTheControlKindFromTheParameterKind)
{
    constexpr SushiDSP::ParameterDescriptor params[] = {
        {"a", "A", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Linear, ""},
        {"b", "B", 0.0f, 1.0f, 0.0f, SushiDSP::ParamKind::Boolean, ""},
        {"c", "C", 0.0f, 3.0f, 0.0f, SushiDSP::ParamKind::Choice, ""},
    };
    const SushiDSP::NodeDescriptor node{"test.node", "Test", params, 3, {1, 1}, nullptr};

    std::vector<ui::ControlPlacement> storage;
    ui::auto_panel(node, storage);

    ASSERT_EQ(storage.size(), 3u);
    EXPECT_EQ(storage[0].kind, ui::ControlKind::Knob);
    EXPECT_EQ(storage[1].kind, ui::ControlKind::Switch);
    EXPECT_EQ(storage[2].kind, ui::ControlKind::Readout);
}

TEST(PanelLibrary, PanelLibraryMixerShowsOneGainKnobPerConnectedInput)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mixer");
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->is_control_visible, nullptr);

    EXPECT_TRUE(panel->is_control_visible(0, 2));
    EXPECT_TRUE(panel->is_control_visible(1, 2));
    EXPECT_FALSE(panel->is_control_visible(2, 2));
    EXPECT_FALSE(panel->is_control_visible(0, 0));
}

TEST(PanelLibrary, PanelLibraryLeavesTheVisibilityRuleUnsetForEveryPanelButTheMixer)
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

TEST(PanelLibrary, PanelLibraryGivesEveryRegisteredNodeAPanelWithAControlPerParameter)
{
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        const SushiDSP::NodeDescriptor& node = *entry.descriptor;
        std::vector<ui::ControlPlacement> storage;
        const ui::PanelDescriptor* hand_authored = ui::panel_for(node.id);
        const ui::PanelDescriptor panel =
            hand_authored != nullptr ? *hand_authored : ui::auto_panel(node, storage);

        // Every parameter of every node has a control. See tests/applications/host_gui/ui/README.md.
        for (std::uint16_t i = 0; i < node.parameter_count; ++i)
        {
            const char* id = node.parameters[i].id;
            bool placed = false;
            for (std::size_t c = 0; c < panel.control_count; ++c)
            {
                if (std::strcmp(panel.controls[c].param_id, id) == 0)
                {
                    placed = true;
                    break;
                }
            }
            EXPECT_TRUE(placed) << node.id << " has no control for '" << id << "'";
        }
    }
}
