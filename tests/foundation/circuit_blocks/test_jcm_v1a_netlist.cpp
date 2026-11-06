/**************************************************************************/
/* test_jcm_v1a_netlist.cpp                                               */
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
 * @file test_jcm_v1a_netlist.cpp
 * @brief Tests that the JCM800 2203 V1a netlist builds in both shapes and names the schematic values.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

TEST(Small_JcmV1aNetlist, SupplyAndGridLeakAreTheSchematicValues)
{
    EXPECT_DOUBLE_EQ(SushiDSP::test_circuits::K_SUPPLY_VOLTS, 360.0);
    EXPECT_DOUBLE_EQ(SushiDSP::test_circuits::K_GRID_LEAK_OHMS, 1.0e6);
}

TEST(Small_JcmV1aNetlist, SixStateBlockBuildsAndNamesAPotentiometer)
{
    SushiDSP::dk::DkNetlist<7, 2, 2, 1> netlist;
    const std::size_t pot = SushiDSP::test_circuits::build_jcm_v1a(netlist);
    EXPECT_LT(pot, SushiDSP::dk::K_MAXIMUM_POTENTIOMETERS);
}

TEST(Small_JcmV1aNetlist, SevenStateBlockWithItsRcTailBuildsAndNamesAPotentiometer)
{
    SushiDSP::dk::DkNetlist<7, 2, 2, 1> netlist;
    const std::size_t pot = SushiDSP::test_circuits::build_jcm_v1a(netlist, true);
    EXPECT_LT(pot, SushiDSP::dk::K_MAXIMUM_POTENTIOMETERS);
}
