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
 * @file test_dk_netlist.cpp
 * @brief Tests the size limits and the empty state of a DK netlist.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

TEST(DkNetlist, SizeLimitsAreTheDocumentedCeilings)
{
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_NODES, 16u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_RESISTORS, 32u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_POTENTIOMETERS, 4u);
    EXPECT_EQ(SushiDSP::dk::K_MAXIMUM_NULLORS, 4u);
}

TEST(DkNetlist, StartsWithNoNodesAboveGround)
{
    const SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
    EXPECT_EQ(netlist.node_count, 0u);
}

TEST(DkNetlist, BranchDefaultsToGroundWithNoValue)
{
    const SushiDSP::dk::Branch branch;
    EXPECT_EQ(branch.positive_node, 0u);
    EXPECT_EQ(branch.negative_node, 0u);
    EXPECT_EQ(branch.value, 0.0);
}
