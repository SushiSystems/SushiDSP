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
 * @brief Tests the alias floor of PentodeStage inside an 8x region.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/oversampled_stage_node.hpp"

#include <SushiDSP/foundation/circuits/stages/pentode_stage.hpp>
#include <SushiDSP/foundation/circuits/stages/power_tube_presets.hpp>
#include <SushiDSP/foundation/regression_test/regression_test.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

namespace
{
    /// @brief A reference pentode for exercising the stage itself.
    ///
    /// The figures are the Philips EL34 datasheet's class B push-pull point, which is a real
    /// operating point rather than any product's tuning. A brick's test that reached into an
    /// amplifier module would fail whenever that amplifier was recalibrated.
    constexpr SushiDSP::PowerTubePreset K_REFERENCE_PENTODE{
        33831.9f,
        220000.0f,
        -38.0f,
        6.882361f,
        SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION,
        SushiDSP::PlateSwing{25.5f, 280.2777f}};

    /// @brief The alias floor in dB of the reference pentode driven to a peak grid voltage.
    double alias_floor_at(float drive_volts)
    {
        TestSupport::OversampledStageNode<SushiDSP::PentodeStage> node(
            SushiDSP::PentodeStage(K_REFERENCE_PENTODE), drive_volts, 300.0f);
        return SushiDSP::measure::alias_floor(node, 48000.0);
    }
} // namespace

SUSHIDSP_REGRESSION_TEST(Small_PentodeStage, PentodeStageAliasFloorInsideItsWindowIsUnderMinusEightyDb)
{
    // The EL34 folds under -80 dB at 8 V. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    EXPECT_LT(alias_floor_at(8.0f), -80.0);
}

SUSHIDSP_REGRESSION_TEST(Small_PentodeStage, PentodeStageAliasFloorAtItsWholeWindowIsUnderMinusFiftyDb)
{
    // The EL34 folds under -50 dB at 38 V. See tests/foundation/circuits/stages/test_pentode_stage/README.md.
    EXPECT_LT(alias_floor_at(38.0f), -50.0);
}
