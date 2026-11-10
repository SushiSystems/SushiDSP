/**************************************************************************/
/* test_level_probe.cpp                                                   */
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
 * @file test_level_probe.cpp
 * @brief Tests that LevelProbe reports and resets the peak magnitude.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

#include "ui/level_probe.hpp"

namespace
{
    /// @brief The length of the block the allocation trap watches one report of.
    constexpr std::size_t K_ALLOCATION_TRAP_BLOCK_SAMPLES = 16384;
} // namespace

TEST(Small_LevelProbe, LevelProbeReportsTheLargestMagnitudeIncludingNegativeSamples)
{
    ui::LevelProbe probe;
    const float block[] = {0.1f, -0.8f, 0.3f};
    probe.report(block, 3);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.8f);
}

TEST(Small_LevelProbe, LevelProbeKeepsTheHighestPeakAcrossSeveralBlocks)
{
    ui::LevelProbe probe;
    const float quiet[] = {0.2f};
    const float loud[] = {0.9f};
    probe.report(quiet, 1);
    probe.report(loud, 1);
    probe.report(quiet, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.9f);
}

TEST(Small_LevelProbe, LevelProbeResetsAfterEachRead)
{
    ui::LevelProbe probe;
    const float block[] = {0.5f};
    probe.report(block, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.5f);
    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}

TEST(Small_LevelProbe, LevelProbeReportsZeroForAnEmptyBlock)
{
    ui::LevelProbe probe;
    probe.report(nullptr, 0);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}

TEST(Small_LevelProbe, ReportPerformsNoHeapAllocation)
{
    ui::LevelProbe probe;
    const std::vector<float> block(K_ALLOCATION_TRAP_BLOCK_SAMPLES, 0.25f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    probe.report(block.data(), static_cast<std::uint32_t>(block.size()));
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
