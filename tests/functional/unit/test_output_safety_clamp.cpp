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

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/output_safety_clamp.hpp"

TEST(Unit_Dsp, OutputSafetyClampAppliesTheGainBeforeTheRail)
{
    std::vector<float> block{0.25f, -0.25f, 0.0f};
    SushiDSP::apply_output_safety_clamp(block.data(), 3, 2.0f);
    EXPECT_FLOAT_EQ(block[0], 0.5f);
    EXPECT_FLOAT_EQ(block[1], -0.5f);
    EXPECT_FLOAT_EQ(block[2], 0.0f);
}

TEST(Unit_Dsp, OutputSafetyClampHoldsBothRails)
{
    std::vector<float> block{4.0f, -4.0f, 1.0f, -1.0f};
    SushiDSP::apply_output_safety_clamp(block.data(), 4, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], -1.0f);
    EXPECT_FLOAT_EQ(block[2], 1.0f);
    EXPECT_FLOAT_EQ(block[3], -1.0f);
}

TEST(Unit_Dsp, OutputSafetyClampLeavesTheTailOfTheBufferAlone)
{
    std::vector<float> block{4.0f, 4.0f, 4.0f};
    SushiDSP::apply_output_safety_clamp(block.data(), 1, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], 4.0f);
    EXPECT_FLOAT_EQ(block[2], 4.0f);
}

TEST(Unit_Dsp, OutputSafetyClampAtZeroFramesTouchesNothing)
{
    std::vector<float> block{4.0f};
    SushiDSP::apply_output_safety_clamp(block.data(), 0, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 4.0f);
}
