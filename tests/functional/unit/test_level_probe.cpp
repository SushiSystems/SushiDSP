/****************************************************************/
/* test_level_probe.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_level_probe.cpp
 * @brief Tests that LevelProbe reports and resets the peak magnitude.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/LevelProbe.hpp"

TEST(Unit_Dsp, LevelProbeReportsTheLargestMagnitudeIncludingNegativeSamples)
{
    ui::LevelProbe probe;
    const float block[] = { 0.1f, -0.8f, 0.3f };
    probe.report(block, 3);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.8f);
}

TEST(Unit_Dsp, LevelProbeKeepsTheHighestPeakAcrossSeveralBlocks)
{
    ui::LevelProbe probe;
    const float quiet[] = { 0.2f };
    const float loud[] = { 0.9f };
    probe.report(quiet, 1);
    probe.report(loud, 1);
    probe.report(quiet, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.9f);
}

TEST(Unit_Dsp, LevelProbeResetsAfterEachRead)
{
    ui::LevelProbe probe;
    const float block[] = { 0.5f };
    probe.report(block, 1);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.5f);
    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}

TEST(Unit_Dsp, LevelProbeReportsZeroForAnEmptyBlock)
{
    ui::LevelProbe probe;
    probe.report(nullptr, 0);

    EXPECT_FLOAT_EQ(probe.read_peak(), 0.0f);
}
