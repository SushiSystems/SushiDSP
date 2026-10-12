/**************************************************************************/
/* test_divider_math.cpp                                                  */
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
 * @file test_divider_math.cpp
 * @brief Tests the drag and fit arithmetic of DividerMath.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/divider_math.hpp"

TEST(Unit_Dsp, DividerMathDragGrowsWithinLimits)
{
    const float size = ui::drag_pane_size(100.0f, 20.0f, 400.0f, {80.0f, 80.0f});
    EXPECT_FLOAT_EQ(size, 120.0f);
}

TEST(Unit_Dsp, DividerMathDragStopsAtTheOtherPanesMinimum)
{
    const float size = ui::drag_pane_size(300.0f, 100.0f, 400.0f, {80.0f, 80.0f});
    EXPECT_FLOAT_EQ(size, 320.0f);
}

TEST(Unit_Dsp, DividerMathDragStopsAtItsOwnMinimum)
{
    const float size = ui::drag_pane_size(100.0f, -200.0f, 400.0f, {80.0f, 80.0f});
    EXPECT_FLOAT_EQ(size, 80.0f);
}

TEST(Unit_Dsp, DividerMathFitShrinksWithTheWindow)
{
    const float size = ui::fit_pane_size(300.0f, 350.0f, {80.0f, 80.0f});
    EXPECT_FLOAT_EQ(size, 270.0f);
}

TEST(Unit_Dsp, DividerMathTooSmallWindowKeepsTheMinimum)
{
    const float size = ui::fit_pane_size(300.0f, 100.0f, {80.0f, 80.0f});
    EXPECT_FLOAT_EQ(size, 80.0f);
}
