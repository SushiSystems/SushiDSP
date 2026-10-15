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
 * @file test_fdn_design.cpp
 * @brief Tests the size limits of the feedback delay network design.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fdn_design.hpp>

#include <cstddef>

TEST(FdnDesign, SixteenLinesAndTwoOutputSections)
{
    EXPECT_EQ(SushiDSP::room::FdnDesign::K_LINE_COUNT, 16u);
    EXPECT_EQ(SushiDSP::room::FdnDesign::K_OUTPUT_SECTIONS, 2u);
}

TEST(FdnDesign, LongestLineIsBelowHalfASecond)
{
    EXPECT_LT(SushiDSP::room::FdnDesign::K_MAX_LINE_SECONDS, 0.5);
    EXPECT_GT(SushiDSP::room::FdnDesign::K_MAX_LINE_SECONDS, 0.0);
}
