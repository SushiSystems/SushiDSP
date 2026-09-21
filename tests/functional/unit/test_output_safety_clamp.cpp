/**************************************************************************/
/* test_output_safety_clamp.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_output_safety_clamp.cpp
 * @brief The output rail's gain, its two limits and the frames it leaves alone.
 * @author Mustafa Garip & Sushi Systems
 */

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/OutputSafetyClamp.hpp"

TEST(Unit_Dsp, OutputSafetyClampAppliesTheGainBeforeTheRail)
{
    std::vector<float> block{ 0.25f, -0.25f, 0.0f };
    SushiDSP::apply_output_safety_clamp(block.data(), 3, 2.0f);
    EXPECT_FLOAT_EQ(block[0], 0.5f);
    EXPECT_FLOAT_EQ(block[1], -0.5f);
    EXPECT_FLOAT_EQ(block[2], 0.0f);
}

TEST(Unit_Dsp, OutputSafetyClampHoldsBothRails)
{
    std::vector<float> block{ 4.0f, -4.0f, 1.0f, -1.0f };
    SushiDSP::apply_output_safety_clamp(block.data(), 4, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], -1.0f);
    EXPECT_FLOAT_EQ(block[2], 1.0f);
    EXPECT_FLOAT_EQ(block[3], -1.0f);
}

TEST(Unit_Dsp, OutputSafetyClampLeavesTheTailOfTheBufferAlone)
{
    std::vector<float> block{ 4.0f, 4.0f, 4.0f };
    SushiDSP::apply_output_safety_clamp(block.data(), 1, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 1.0f);
    EXPECT_FLOAT_EQ(block[1], 4.0f);
    EXPECT_FLOAT_EQ(block[2], 4.0f);
}

TEST(Unit_Dsp, OutputSafetyClampAtZeroFramesTouchesNothing)
{
    std::vector<float> block{ 4.0f };
    SushiDSP::apply_output_safety_clamp(block.data(), 0, 1.0f);
    EXPECT_FLOAT_EQ(block[0], 4.0f);
}
