/**************************************************************************/
/* test_chain_layout.cpp                                                  */
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

#include "ui/ChainLayout.hpp"

#include <algorithm>

namespace
{
    /// @brief Finds a slot's cell, so a test asserts on identity rather than on cell order.
    const ui::ChainCell& cell_for(const ui::ChainView& view, RigController::SlotId slot)
    {
        const auto it = std::find_if(view.cells.begin(), view.cells.end(),
            [slot](const ui::ChainCell& cell) { return cell.slot == slot; });
        EXPECT_NE(it, view.cells.end());
        return *it;
    }
}

TEST(Unit_Dsp, ChainLayoutPlacesALinearChainOnOneRowWithAscendingColumns)
{
    const ui::ChainView view = ui::layout_chain({ 10, 11, 12 }, { { 10, 11 }, { 11, 12 } });

    EXPECT_EQ(view.cells.size(), 3u);
    EXPECT_EQ(view.row_count, 1);
    EXPECT_EQ(view.column_count, 3);
    EXPECT_EQ(cell_for(view, 10).column, 0);
    EXPECT_EQ(cell_for(view, 11).column, 1);
    EXPECT_EQ(cell_for(view, 12).column, 2);
    EXPECT_EQ(cell_for(view, 10).row, 0);
    EXPECT_EQ(cell_for(view, 11).row, 0);
    EXPECT_EQ(cell_for(view, 12).row, 0);
    EXPECT_EQ(view.links.size(), 2u);
}

TEST(Unit_Dsp, ChainLayoutPutsTheSecondBranchOnRowOneAndRejoinsAtRowZero)
{
    // 1 -> splitter 2 -> branches 3 and 4 -> mixer 5
    const ui::ChainView view = ui::layout_chain(
        { 1, 2, 3, 4, 5 },
        { { 1, 2 }, { 2, 3 }, { 2, 4 }, { 3, 5 }, { 4, 5 } });

    EXPECT_EQ(view.row_count, 2);
    EXPECT_EQ(view.column_count, 4);
    EXPECT_EQ(cell_for(view, 1).row, 0);
    EXPECT_EQ(cell_for(view, 2).row, 0);
    EXPECT_EQ(cell_for(view, 3).row, 0);
    EXPECT_EQ(cell_for(view, 4).row, 1);
    EXPECT_EQ(cell_for(view, 5).row, 0);
    EXPECT_EQ(cell_for(view, 5).column, 3);
    EXPECT_EQ(view.links.size(), 5u);
}

TEST(Unit_Dsp, ChainLayoutPushesTheRejoinRightOfTheLongestBranch)
{
    // Branch A is two nodes long (3, 6); branch B is one node (4).
    const ui::ChainView view = ui::layout_chain(
        { 2, 3, 4, 5, 6 },
        { { 2, 3 }, { 2, 4 }, { 3, 6 }, { 6, 5 }, { 4, 5 } });

    EXPECT_EQ(cell_for(view, 2).column, 0);
    EXPECT_EQ(cell_for(view, 3).column, 1);
    EXPECT_EQ(cell_for(view, 6).column, 2);
    EXPECT_EQ(cell_for(view, 4).column, 1);
    EXPECT_EQ(cell_for(view, 5).column, 3);
    EXPECT_EQ(view.column_count, 4);
}

TEST(Unit_Dsp, ChainLayoutGivesAnEmptyViewForAnEmptyRig)
{
    const ui::ChainView view = ui::layout_chain({}, {});

    EXPECT_TRUE(view.cells.empty());
    EXPECT_TRUE(view.links.empty());
    EXPECT_EQ(view.row_count, 0);
    EXPECT_EQ(view.column_count, 0);
}

TEST(Unit_Dsp, ChainLayoutPlacesAnUnconnectedSlotInTheFirstColumn)
{
    const ui::ChainView view = ui::layout_chain({ 7, 8 }, {});

    EXPECT_EQ(cell_for(view, 7).column, 0);
    EXPECT_EQ(cell_for(view, 8).column, 0);
    EXPECT_EQ(view.column_count, 1);
}

TEST(Unit_Dsp, ChainLayoutLinksIndexTheCellsTheirEdgeConnects)
{
    const ui::ChainView view = ui::layout_chain({ 10, 11 }, { { 10, 11 } });

    ASSERT_EQ(view.links.size(), 1u);
    EXPECT_EQ(view.cells[view.links[0].from_cell].slot, 10u);
    EXPECT_EQ(view.cells[view.links[0].to_cell].slot, 11u);
}
