/****************************************************************/
/* test_tap_tempo.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_tap_tempo.cpp
 * @brief Tests the tempo TapTempo reads from taps.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/TapTempo.hpp"

TEST(Unit_Dsp, TapTempoReadsFourEvenTapsAsTheirTempo)
{
    ui::TapTempo taps;

    EXPECT_FALSE(taps.tap(0.0).has_value());

    const std::optional<double> second = taps.tap(0.5);
    ASSERT_TRUE(second.has_value());
    EXPECT_DOUBLE_EQ(*second, 120.0);

    EXPECT_DOUBLE_EQ(*taps.tap(1.0), 120.0);
    EXPECT_DOUBLE_EQ(*taps.tap(1.5), 120.0);
    EXPECT_EQ(taps.tap_count(), ui::TapTempo::kTapCount);
}

TEST(Unit_Dsp, TapTempoAveragesOverTheLastFourTapsOnly)
{
    ui::TapTempo taps;

    taps.tap(0.0);
    taps.tap(1.0);
    taps.tap(2.0);
    taps.tap(3.0);
    EXPECT_DOUBLE_EQ(*taps.tap(3.5), 72.0);

    // The window now holds 2.0, 3.0, 3.5 and 4.0; the 0.0 and 1.0 taps have left it.
    EXPECT_DOUBLE_EQ(*taps.tap(4.0), 90.0);
}

TEST(Unit_Dsp, TapTempoStartsAgainAfterAGapOverTwoSeconds)
{
    ui::TapTempo taps;

    taps.tap(0.0);
    taps.tap(0.5);

    EXPECT_FALSE(taps.tap(10.0).has_value());
    EXPECT_EQ(taps.tap_count(), 1u);
    EXPECT_NEAR(*taps.tap(10.4), 150.0, 1e-9);
}

TEST(Unit_Dsp, TapTempoHoldsItsTempoInsideTheRange)
{
    ui::TapTempo taps;

    taps.tap(0.0);
    EXPECT_DOUBLE_EQ(*taps.tap(0.1), ui::TapTempo::kMaximumTempo);

    taps.reset();
    EXPECT_EQ(taps.tap_count(), 0u);
    taps.tap(0.0);
    EXPECT_DOUBLE_EQ(*taps.tap(2.0), ui::TapTempo::kMinimumTempo);
}
