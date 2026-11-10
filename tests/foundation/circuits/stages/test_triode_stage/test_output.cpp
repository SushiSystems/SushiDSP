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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/oversampled_stage_node.hpp"

#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>

namespace
{
    /// @brief Holds one tone of the 1 to 20 kHz sweep at one drive, with the bound DA5 spec section
    /// 11 sets for that drive.
    struct AliasFloorRow
    {
            /// @brief Stores the peak grid drive in volts, 3x or 12x the stage's 2.0 V window.
            std::uint32_t drive_volts;

            /// @brief Stores the bound in dB the tone's alias floor stays under.
            double bound_db;

            /// @brief Stores the tone in hertz, a multiple of 1 kHz.
            std::uint32_t tone_hz;
    };

    /// @brief Counts the tones of the sweep, 1 kHz to 20 kHz in steps of 1 kHz, as
    /// SushiDSP::measure::alias_floor walks them.
    constexpr std::uint32_t K_SWEEP_TONES = 20;

    /// @brief Returns the rows of both drives, one per tone: 6 V under -60 dB and 24 V under -40 dB.
    /// The worst tone of a sweep is under a bound exactly when every tone is, so the rows together
    /// hold what one sweep held, one render of a second and one analysis frame at a time.
    std::vector<AliasFloorRow> alias_floor_rows()
    {
        std::vector<AliasFloorRow> rows;
        for (std::uint32_t tone = 1; tone <= K_SWEEP_TONES; ++tone)
        {
            rows.push_back({6u, -60.0, 1000u * tone});
        }
        for (std::uint32_t tone = 1; tone <= K_SWEEP_TONES; ++tone)
        {
            rows.push_back({24u, -40.0, 1000u * tone});
        }
        return rows;
    }

    /// @brief Names a row by its drive and tone.
    std::string alias_floor_row_name(const ::testing::TestParamInfo<AliasFloorRow>& info)
    {
        return "Drive" + std::to_string(static_cast<std::size_t>(info.param.drive_volts)) + "VAt" +
               std::to_string(static_cast<std::size_t>(info.param.tone_hz)) + "Hz";
    }

    /// @brief The alias floor in dB of the V5A stage driven to a peak grid voltage by one tone.
    double alias_floor_at(std::uint32_t drive_volts, std::uint32_t tone_hz)
    {
        TestSupport::OversampledStageNode<SushiDSP::TriodeStage> node(
            SushiDSP::TriodeStage(671730.0f, 1000000.0f, -2.0f, 25.3f,
                                  SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                                  SushiDSP::PlateSwing{90.742f, 134.001f}),
            static_cast<float>(drive_volts), 134.0f);
        return SushiDSP::measure::alias_floor_at_tone(node, 48000.0, static_cast<double>(tone_hz));
    }
} // namespace

class Medium_TriodeStageAliasFloor : public ::testing::TestWithParam<AliasFloorRow>
{
};

TEST_P(Medium_TriodeStageAliasFloor, TriodeStageAliasFloorOfOneToneIsUnderTheBoundOfItsDrive)
{
    // V5A alone folds under -60 dB at 6 V and under -40 dB at 24 V. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const AliasFloorRow& row = GetParam();
    EXPECT_LT(alias_floor_at(row.drive_volts, row.tone_hz), row.bound_db);
}

INSTANTIATE_TEST_SUITE_P(Sweep, Medium_TriodeStageAliasFloor,
                         ::testing::ValuesIn(alias_floor_rows()), alias_floor_row_name);
