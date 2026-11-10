/**************************************************************************/
/* test_slot_selection.cpp                                                */
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
 * @file test_slot_selection.cpp
 * @brief Tests which rack-list selections slot_selection keeps.
 * @author Mustafa Garip
 */

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "slot_graph.hpp"
#include "slot_selection.hpp"

namespace
{
    /// @brief Holds one selection, the live slots it is checked against and whether it stays.
    struct SelectionRow
    {
            const char* name;
            std::optional<SlotGraph::SlotId> selection;
            std::vector<SlotGraph::SlotId> live;
            bool stays;
    };

    class Small_SlotSelection : public ::testing::TestWithParam<SelectionRow>
    {
    };
} // namespace

TEST_P(Small_SlotSelection, StillValidKeepsASelectionOnlyWhileItsSlotIsLive)
{
    EXPECT_EQ(slot_selection::still_valid(GetParam().selection, GetParam().live),
              GetParam().stays);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_SlotSelection,
    ::testing::Values(SelectionRow{"SlotInTheList", 7, {3, 7, 9}, true},
                      SelectionRow{"SlotRemoved", 7, {3, 9}, false},
                      SelectionRow{"NoSelection", std::nullopt, {3, 7, 9}, false}),
    [](const ::testing::TestParamInfo<SelectionRow>& info) { return std::string(info.param.name); });
