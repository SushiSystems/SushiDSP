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
 * @brief Golden-vector regression tests for the DiezelVh4 node.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/diezel_vh4/diezel_vh4.hpp>
#include <SushiDSP/foundation/golden_compare/golden_file.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief The residual, in dB against the golden file, a render may reach.
    /// Characterized: the golden is the code's own output, so the bound only has to cover the
    /// last-bit differences between builds that the saturating chain amplifies; -80 dB is the
    /// bound the golden tests of every node use. A missing or short file reads 0 dB and fails.
    constexpr double K_GOLDEN_RESIDUAL_BOUND_DB = -80.0;

    /// @brief Names a golden file and the rate it was rendered at.
    struct GoldenRow
    {
            const char* file;
            double sample_rate;
    };

    const GoldenRow K_GOLDEN_ROWS[] = {{"diezel_vh4_48k", 48000.0}, {"diezel_vh4_96k", 96000.0}};

    /// @brief Renders one second of the amp at the knob positions the snapshot pins.
    ///
    /// Every knob is set before the render prepares the node, so the smoothers snap and the file
    /// holds the settled amp.
    /// @param sample_rate Rate to render at.
    /// @return sample_rate samples of output.
    std::vector<float> render_diezel_vh4(double sample_rate)
    {
        SushiDSP::DiezelVh4 node;
        node.set_gain(0.6f);
        node.set_bass(0.5f);
        node.set_middle(0.5f);
        node.set_treble(0.6f);
        node.set_volume(0.5f);
        node.set_deep(0.5f);
        node.set_presence(0.5f);
        node.set_master(0.0f);
        return SushiDSP::measure::render(node, SushiDSP::golden::stimulus(sample_rate), sample_rate, K_BLOCK);
    }
} // namespace

/// @brief Tests that the render at each rate matches its golden file within the bound.
/// @see tests/nodes/diezel_vh4/test_diezel_vh4/README.md
TEST(Medium_DiezelVh4, DiezelVh4GoldenFileMatchesAtEachRate)
{
    for (const GoldenRow& row : K_GOLDEN_ROWS)
    {
        EXPECT_LE(SushiDSP::golden::compare_or_regenerate(SUSHIDSP_GOLDEN_DIR, row.file, render_diezel_vh4(row.sample_rate)),
                  K_GOLDEN_RESIDUAL_BOUND_DB)
            << row.file;
    }
}
