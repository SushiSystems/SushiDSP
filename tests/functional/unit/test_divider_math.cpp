/****************************************************************/
/* test_divider_math.cpp                                        */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_divider_math.cpp
 * @brief Tests the drag and fit arithmetic of DividerMath.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/DividerMath.hpp"

TEST(Unit_Dsp, DividerMathDragGrowsWithinLimits)
{
    const float size = ui::drag_pane_size(100.0f, 20.0f, 400.0f, { 80.0f, 80.0f });
    EXPECT_FLOAT_EQ(size, 120.0f);
}

TEST(Unit_Dsp, DividerMathDragStopsAtTheOtherPanesMinimum)
{
    const float size = ui::drag_pane_size(300.0f, 100.0f, 400.0f, { 80.0f, 80.0f });
    EXPECT_FLOAT_EQ(size, 320.0f);
}

TEST(Unit_Dsp, DividerMathDragStopsAtItsOwnMinimum)
{
    const float size = ui::drag_pane_size(100.0f, -200.0f, 400.0f, { 80.0f, 80.0f });
    EXPECT_FLOAT_EQ(size, 80.0f);
}

TEST(Unit_Dsp, DividerMathFitShrinksWithTheWindow)
{
    const float size = ui::fit_pane_size(300.0f, 350.0f, { 80.0f, 80.0f });
    EXPECT_FLOAT_EQ(size, 270.0f);
}

TEST(Unit_Dsp, DividerMathTooSmallWindowKeepsTheMinimum)
{
    const float size = ui::fit_pane_size(300.0f, 100.0f, { 80.0f, 80.0f });
    EXPECT_FLOAT_EQ(size, 80.0f);
}
