/**************************************************************************/
/* test_output.cpp                                                        */
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
 * @file test_output.cpp
 * @brief Tests the alias floor of TriodeStage inside an 8x region.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/oversampled_stage_node.hpp"

#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>
#include <SushiDSP/foundation/regression_test/regression_test.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

namespace
{
    /// @brief The alias floor in dB of the V5A stage driven to a peak grid voltage.
    double alias_floor_at(float drive_volts)
    {
        TestSupport::OversampledStageNode<SushiDSP::TriodeStage> node(
            SushiDSP::TriodeStage(671730.0f, 1000000.0f, -2.0f, 25.3f,
                                  SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                                  SushiDSP::PlateSwing{90.742f, 134.001f}),
            drive_volts, 134.0f);
        return SushiDSP::measure::alias_floor(node, 48000.0);
    }
} // namespace

SUSHIDSP_REGRESSION_TEST(Medium_TriodeStage, TriodeStageAliasFloorAtThreeTimesItsWindowIsUnderMinusSixtyDb)
{
    // V5A alone folds under -60 dB at 6 V. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    EXPECT_LT(alias_floor_at(6.0f), -60.0);
}

SUSHIDSP_REGRESSION_TEST(Medium_TriodeStage, TriodeStageAliasFloorAtTwelveTimesItsWindowIsUnderMinusFortyDb)
{
    // V5A alone folds under -40 dB at 24 V. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    EXPECT_LT(alias_floor_at(24.0f), -40.0);
}
