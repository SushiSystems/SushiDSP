/**************************************************************************/
/* test_grid_conduction_law.cpp                                           */
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
 * @file test_grid_conduction_law.cpp
 * @brief Tests that a grid conduction law holds the three constants it is built from.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/grid_conduction_law.hpp>

TEST(Small_GridConductionLaw, KeepsScaleThresholdAndTransitionAsGiven)
{
    const SushiDSP::GridConductionLaw law{1.0e-6f, 0.5f, 0.15f};
    EXPECT_FLOAT_EQ(law.scale_amps, 1.0e-6f);
    EXPECT_FLOAT_EQ(law.threshold_volts, 0.5f);
    EXPECT_FLOAT_EQ(law.transition_volts, 0.15f);
}
