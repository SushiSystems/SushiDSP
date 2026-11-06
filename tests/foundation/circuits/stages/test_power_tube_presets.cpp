/**************************************************************************/
/* test_power_tube_presets.cpp                                            */
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
 * @file test_power_tube_presets.cpp
 * @brief Tests that the EL34 grid conduction preset carries the pentode model's constants.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/power_tube_presets.hpp>
#include <SushiDSP/foundation/circuits/models/pentode_el34.hpp>

TEST(Small_PowerTubePresets, El34GridConductionMatchesThePentodeModel)
{
    using SushiDSP::PentodeEL34;
    const SushiDSP::GridConductionLaw& law = SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION;
    EXPECT_FLOAT_EQ(law.scale_amps, static_cast<float>(PentodeEL34::K_GRID_CURRENT_SCALE));
    EXPECT_FLOAT_EQ(law.threshold_volts, static_cast<float>(PentodeEL34::K_GRID_THRESHOLD_VOLTS));
    EXPECT_FLOAT_EQ(law.transition_volts, static_cast<float>(PentodeEL34::K_GRID_TRANSITION_VOLTS));
}
