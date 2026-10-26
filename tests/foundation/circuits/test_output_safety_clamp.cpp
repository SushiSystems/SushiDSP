/**************************************************************************/
/* test_output_safety_clamp.cpp                                           */
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
 * @file test_output_safety_clamp.cpp
 * @brief The output rail's gain, its two limits and the frames it leaves alone.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/output_safety_clamp.hpp>

TEST(Small_OutputSafetyClamp, OutputSafetyClampAppliesTheGainBeforeTheRail)
{
    std::vector<float> block{0.25f, -0.25f, 0.0f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 3, 2.0f), 0u);
    EXPECT_FLOAT_EQ(block[0], 0.5f);
    EXPECT_FLOAT_EQ(block[1], -0.5f);
    EXPECT_FLOAT_EQ(block[2], 0.0f);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampHoldsBothRails)
{
    std::vector<float> block{4.0f, -4.0f, 1.0f, -1.0f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 4, 1.0f), 2u);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], -1.0f);
    EXPECT_FLOAT_EQ(block[2], 1.0f);
    EXPECT_FLOAT_EQ(block[3], -1.0f);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampLeavesTheTailOfTheBufferAlone)
{
    std::vector<float> block{4.0f, 4.0f, 4.0f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 1, 1.0f), 1u);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], 4.0f);
    EXPECT_FLOAT_EQ(block[2], 4.0f);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampAtZeroFramesTouchesNothing)
{
    std::vector<float> block{4.0f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 0, 1.0f), 0u);
    EXPECT_FLOAT_EQ(block[0], 4.0f);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampCountsEachSampleItCutsAndNoOther)
{
    std::vector<float> block{0.5f, 1.5f, -1.5f, 0.999f, 2.0f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 5, 1.0f), 3u);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampCountsANaNAndPassesItUnchanged)
{
    std::vector<float> block{0.25f, std::numeric_limits<float>::quiet_NaN(), 0.25f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 3, 1.0f), 1u);
    EXPECT_TRUE(std::isnan(block[1]));
    EXPECT_FLOAT_EQ(block[0], 0.25f);
}

TEST(Small_OutputSafetyClamp, OutputSafetyClampCountsAnInfinityOnceAndHoldsItAtTheRail)
{
    std::vector<float> block{std::numeric_limits<float>::infinity(), 0.1f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 2, 1.0f), 1u);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
}

TEST(Small_OutputSafetyClamp, AGainThatPushesAQuietBlockPastTheRailIsCounted)
{
    std::vector<float> block{0.6f, 0.6f, 0.1f};
    EXPECT_EQ(SushiDSP::apply_output_safety_clamp(block.data(), 3, 2.0f), 2u);
}
