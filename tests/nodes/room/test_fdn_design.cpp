/**************************************************************************/
/* test_fdn_design.cpp                                                    */
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
 * @file test_fdn_design.cpp
 * @brief Tests the size limits of the feedback delay network design.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fdn_design.hpp>

#include <cstddef>

TEST(Small_FdnDesign, SixteenLinesAndTwoOutputSections)
{
    EXPECT_EQ(SushiDSP::room::FdnDesign::K_LINE_COUNT, 16u);
    EXPECT_EQ(SushiDSP::room::FdnDesign::K_OUTPUT_SECTIONS, 2u);
}

TEST(Small_FdnDesign, LongestLineBoundIsPositiveAndBelowOneSecond)
{
    EXPECT_LT(SushiDSP::room::FdnDesign::K_MAX_LINE_SECONDS, 1.0);
    EXPECT_GT(SushiDSP::room::FdnDesign::K_MAX_LINE_SECONDS, 0.0);
}
