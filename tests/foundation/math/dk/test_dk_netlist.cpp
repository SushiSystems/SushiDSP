/**************************************************************************/
/* test_dk_netlist.cpp                                                    */
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
 * @file test_dk_netlist.cpp
 * @brief Tests the size limits and the empty state of a DK netlist.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

TEST(Small_DkNetlist, SizeLimitsAreTheDocumentedCeilings)
{
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_NODES, 16u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_RESISTORS, 32u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_POTENTIOMETERS, 4u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_NULLORS, 4u);
}

TEST(Small_DkNetlist, StartsWithNoNodesAboveGround)
{
    const SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
    EXPECT_EQ(netlist.node_count, 0u);
}

TEST(Small_DkNetlist, BranchDefaultsToGroundWithNoValue)
{
    const SushiDSP::dk::Branch branch;
    EXPECT_EQ(branch.positive_node, 0u);
    EXPECT_EQ(branch.negative_node, 0u);
    EXPECT_EQ(branch.value, 0.0);
}
