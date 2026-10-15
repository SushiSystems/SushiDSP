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
 * @file test_level_probe.cpp
 * @brief Tests that LevelProbe reports and resets the peak magnitude.
 * @author Mustafa Garip
 */

#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

#include "ui/level_probe.hpp"

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
    std::vector<float> block(256, 0.25f);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int i = 0; i < 64; ++i)
    {
        probe.report(block.data(), 256);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
