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
 * @brief Tests the TubeScreamer node against its golden files and its alias-floor regression floor.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/foundation/golden_compare/golden_file.hpp>
#include <SushiDSP/foundation/signal_measure/alias_floor.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief The residual, in dB against the golden file, a render may reach.
    /// Characterized: the golden is the code's own output, so the bound only has to cover the
    /// last-bit differences between builds that the clipping chain amplifies; -80 dB is the
    /// bound the golden tests of every node use. A missing or short file reads 0 dB and fails.
    constexpr double K_GOLDEN_RESIDUAL_BOUND_DB = -80.0;

    /// @brief The alias floor the pedal may not exceed at drive 1, in dB. Characterized: the
    /// measured floor is -18.6 dB (README) with 1 dB of slack.
    constexpr double K_ALIAS_FLOOR_BOUND_DB = -17.6;

    /// @brief Names a golden file and the rate it was rendered at.
    struct GoldenRow
    {
            const char* file;
            double sample_rate;
    };

    const GoldenRow K_GOLDEN_ROWS[] = {{"tube_screamer_48k", 48000.0}, {"tube_screamer_96k", 96000.0}};

    /// @brief Renders one second of the pedal at the knob positions the snapshot pins.
    ///
    /// Every knob is set before the render prepares the node, so the smoothers snap and the file
    /// holds the settled pedal.
    /// @param sample_rate Rate to render at.
    /// @return sample_rate samples of output.
    std::vector<float> render_tube_screamer(double sample_rate)
    {
        SushiDSP::TubeScreamer node;
        node.set_drive(0.6f);
        node.set_tone(0.4f);
        node.set_level(SushiDSP::gain_to_decibels(0.8f / SushiDSP::TubeScreamer::K_LEVEL_UNITY_GAIN));
        return SushiDSP::measure::render(node, SushiDSP::golden::stimulus(sample_rate), sample_rate, K_BLOCK);
    }
} // namespace

/// @brief Tests that the render at each rate matches its golden file within the bound.
/// @see tests/nodes/tubescreamer/test_tube_screamer/README.md
TEST(Medium_TubeScreamer, TubeScreamerGoldenFileMatchesAtEachRate)
{
    for (const GoldenRow& row : K_GOLDEN_ROWS)
    {
        EXPECT_LE(SushiDSP::golden::compare_or_regenerate(SUSHIDSP_GOLDEN_DIR, row.file, render_tube_screamer(row.sample_rate)),
                  K_GOLDEN_RESIDUAL_BOUND_DB)
            << row.file;
    }
}

TEST(Medium_TubeScreamer, TubeScreamerAliasFloorDoesNotRegress)
{
    // The floor is the measured -18.6 dB with 1 dB of slack. See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    SushiDSP::TubeScreamer node;
    node.set_drive(1.0f);
    node.set_tone(0.5f);
    EXPECT_LE(SushiDSP::measure::alias_floor(node, 48000.0), K_ALIAS_FLOOR_BOUND_DB);
}
