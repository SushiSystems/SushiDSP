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
 * @file test_panel_library.cpp
 * @brief Tests that PanelLibrary returns a panel for every registered node.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <SushiDSP/nodes/jcm800/jcm800.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

#include "ui/panel_library.hpp"

namespace
{
    /// @brief One node type a parameterised case runs for: a test-name-safe name and the id.
    struct NodeRow
    {
            std::string name;
            std::string id;
    };

    /// @brief Turns a node id into a name made of letters, digits and underscores.
    std::string safe_name(const std::string& id)
    {
        std::string name = id;
        for (char& letter : name)
        {
            letter = std::isalnum(static_cast<unsigned char>(letter)) != 0 ? letter : '_';
        }
        return name;
    }

    /// @brief Returns the row's name, for the parameterised test names.
    std::string row_name(const ::testing::TestParamInfo<NodeRow>& info)
    {
        return info.param.name;
    }

    /// @brief Makes a row of a node id.
    NodeRow row_of(const char* id)
    {
        return NodeRow{safe_name(id), id};
    }

    /// @brief One row per registered node whose category is the given one.
    std::vector<NodeRow> registered_rows(const char* category)
    {
        std::vector<NodeRow> rows;
        for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
        {
            if (category == nullptr || std::strcmp(entry.category.name, category) == 0)
            {
                rows.push_back(row_of(entry.descriptor->id));
            }
        }
        return rows;
    }

    /// @brief One row per hand-authored panel, leaving out the panel of the id given.
    std::vector<NodeRow> panel_rows_except(const char* excluded_id)
    {
        std::vector<NodeRow> rows;
        for (const ui::PanelDescriptor& panel : ui::panel_table())
        {
            if (excluded_id == nullptr || std::strcmp(panel.descriptor_id, excluded_id) != 0)
            {
                rows.push_back(row_of(panel.descriptor_id));
            }
        }
        return rows;
    }

    /// @brief Finds the registered descriptor of an id.
    /// @return The descriptor, or nullptr when the registry does not hold the id.
    const SushiDSP::NodeDescriptor* registered_descriptor(const std::string& id)
    {
        for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
        {
            if (id == entry.descriptor->id)
            {
                return entry.descriptor;
            }
        }
        return nullptr;
    }

    /// @brief Lists the parameter ids of a descriptor that a panel has no control for.
    std::vector<std::string> parameters_without_control(const ui::PanelDescriptor& panel,
                                                        const SushiDSP::NodeDescriptor& node)
    {
        std::vector<std::string> missing;
        for (std::uint16_t index = 0; index < node.parameter_count; ++index)
        {
            const char* id = node.parameters[index].id;
            bool placed = false;
            for (std::size_t control = 0; control < panel.control_count; ++control)
            {
                placed = placed || std::strcmp(panel.controls[control].param_id, id) == 0;
            }
            if (!placed)
            {
                missing.push_back(id);
            }
        }
        return missing;
    }

    /// @brief Lists the control parameter ids of a panel that a descriptor does not declare.
    std::vector<std::string> controls_without_parameter(const ui::PanelDescriptor& panel,
                                                        const SushiDSP::NodeDescriptor& node)
    {
        std::vector<std::string> stray;
        for (std::size_t control = 0; control < panel.control_count; ++control)
        {
            const char* id = panel.controls[control].param_id;
            bool declared = false;
            for (std::uint16_t index = 0; index < node.parameter_count; ++index)
            {
                declared = declared || std::strcmp(node.parameters[index].id, id) == 0;
            }
            if (!declared)
            {
                stray.push_back(id);
            }
        }
        return stray;
    }

    /// @brief Lists the parameter ids of the sliders a panel draws, in control order.
    std::vector<std::string> slider_ids(const ui::PanelDescriptor& panel)
    {
        std::vector<std::string> ids;
        for (std::size_t control = 0; control < panel.control_count; ++control)
        {
            if (panel.controls[control].kind == ui::ControlKind::Slider)
            {
                ids.push_back(panel.controls[control].param_id);
            }
        }
        return ids;
    }

    /// @brief Counts the runs of placements that share a y position.
    std::size_t distinct_rows(const std::vector<ui::ControlPlacement>& placements)
    {
        std::size_t rows = 0;
        float previous_y = -1.0f;
        for (const ui::ControlPlacement& placement : placements)
        {
            rows += placement.position.y != previous_y ? 1u : 0u;
            previous_y = placement.position.y;
        }
        return rows;
    }

    /// @brief The hand-authored panel of a node, or its generic panel when it has none.
    ui::PanelDescriptor panel_of(const SushiDSP::NodeDescriptor& node,
                                 std::vector<ui::ControlPlacement>& storage)
    {
        const ui::PanelDescriptor* hand_authored = ui::panel_for(node.id);
        return hand_authored != nullptr ? *hand_authored : ui::auto_panel(node, storage);
    }
} // namespace

TEST(Small_PanelLibrary, PanelLibraryReturnsAHandAuthoredPanelForAKnownNodeType)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.jcm800");
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->control_count, 8u);
    EXPECT_GT(panel->aspect.x, panel->aspect.y);
}

TEST(Small_PanelLibrary, PanelLibraryRegistryListsAtLeastOneAmplifier)
{
    EXPECT_FALSE(registered_rows("Amp").empty());
}

class Small_PanelLibraryAmplifier : public ::testing::TestWithParam<NodeRow>
{
};

TEST_P(Small_PanelLibraryAmplifier, PanelLibraryReturnsAHandAuthoredPanelForTheAmplifier)
{
    // The registry says which products are amplifiers. See tests/applications/host_gui/ui/README.md.
    const ui::PanelDescriptor* panel = ui::panel_for(GetParam().id.c_str());

    ASSERT_NE(panel, nullptr);
    EXPECT_GT(panel->control_count, 0u);
    EXPECT_GT(panel->aspect.x, panel->aspect.y);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_PanelLibraryAmplifier, ::testing::ValuesIn(registered_rows("Amp")),
                         row_name);

TEST(Small_PanelLibrary, PanelLibraryDrawsTheMarkTwoCPlusEqualizerBandsAsFaders)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mesa_mark2c");
    ASSERT_NE(panel, nullptr);

    const std::vector<std::string> expected{"eq_80", "eq_240", "eq_750", "eq_2200", "eq_6600"};
    EXPECT_EQ(slider_ids(*panel), expected);
}

TEST(Small_PanelLibrary, PanelLibraryReturnsNullForANodeTypeWithNoEntry)
{
    EXPECT_EQ(ui::panel_for("sushidsp.not_a_real_node"), nullptr);
}

class Small_PanelLibraryTableEntry : public ::testing::TestWithParam<NodeRow>
{
};

TEST_P(Small_PanelLibraryTableEntry, PanelLibraryTableNamesOnlyParametersTheDescriptorDeclares)
{
    const SushiDSP::NodeDescriptor* node = registered_descriptor(GetParam().id);
    const ui::PanelDescriptor* panel = ui::panel_for(GetParam().id.c_str());
    ASSERT_NE(node, nullptr);
    ASSERT_NE(panel, nullptr);

    EXPECT_EQ(controls_without_parameter(*panel, *node), std::vector<std::string>{});
}

TEST_P(Small_PanelLibraryTableEntry, PanelLibraryEveryTableEntryNamesARegisteredNodeType)
{
    EXPECT_NE(registered_descriptor(GetParam().id), nullptr);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_PanelLibraryTableEntry,
                         ::testing::ValuesIn(panel_rows_except(nullptr)), row_name);

TEST(Small_PanelLibrary, PanelLibraryAutoPanelPlacesOneControlPerParameter)
{
    const SushiDSP::NodeDescriptor& jcm = SushiDSP::jcm800_descriptor();
    std::vector<ui::ControlPlacement> storage;
    const ui::PanelDescriptor panel = ui::auto_panel(jcm, storage);

    EXPECT_EQ(panel.control_count, jcm.parameter_count);
    EXPECT_EQ(storage.size(), jcm.parameter_count);
}

TEST(Small_PanelLibrary, PanelLibraryAutoPanelLaysEightParametersOutInThreeRows)
{
    const SushiDSP::NodeDescriptor& jcm = SushiDSP::jcm800_descriptor();
    std::vector<ui::ControlPlacement> storage;
    static_cast<void>(ui::auto_panel(jcm, storage));

    // 8 parameters fit a 3-column grid, so rows 0, 1 and 2 are used.
    EXPECT_EQ(distinct_rows(storage), 3u);
}

TEST(Small_PanelLibrary, PanelLibraryAutoPanelPicksTheControlKindFromTheParameterKind)
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

TEST(Small_PanelLibrary, PanelLibraryMixerShowsOneGainKnobPerConnectedInput)
{
    const ui::PanelDescriptor* panel = ui::panel_for("sushidsp.mixer");
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->is_control_visible, nullptr);

    EXPECT_TRUE(panel->is_control_visible(0, 2));
    EXPECT_TRUE(panel->is_control_visible(1, 2));
    EXPECT_FALSE(panel->is_control_visible(2, 2));
    EXPECT_FALSE(panel->is_control_visible(0, 0));
}

class Small_PanelLibraryNonMixerPanel : public ::testing::TestWithParam<NodeRow>
{
};

TEST_P(Small_PanelLibraryNonMixerPanel, PanelLibraryLeavesTheVisibilityRuleUnsetForThePanel)
{
    const ui::PanelDescriptor* panel = ui::panel_for(GetParam().id.c_str());
    ASSERT_NE(panel, nullptr);

    EXPECT_EQ(panel->is_control_visible, nullptr);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_PanelLibraryNonMixerPanel,
                         ::testing::ValuesIn(panel_rows_except("sushidsp.mixer")), row_name);

class Small_PanelLibraryRegisteredNode : public ::testing::TestWithParam<NodeRow>
{
};

TEST_P(Small_PanelLibraryRegisteredNode, PanelLibraryGivesTheNodeAPanelWithAControlPerParameter)
{
    const SushiDSP::NodeDescriptor* node = registered_descriptor(GetParam().id);
    ASSERT_NE(node, nullptr);
    std::vector<ui::ControlPlacement> storage;
    const ui::PanelDescriptor panel = panel_of(*node, storage);

    // Every parameter of every node has a control. See tests/applications/host_gui/ui/README.md.

    EXPECT_EQ(parameters_without_control(panel, *node), std::vector<std::string>{});
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_PanelLibraryRegisteredNode,
                         ::testing::ValuesIn(registered_rows(nullptr)), row_name);
