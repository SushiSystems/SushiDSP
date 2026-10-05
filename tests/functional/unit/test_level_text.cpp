/****************************************************************/
/* test_level_text.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_level_text.cpp
 * @brief Tests the decibel text and meter fill of LevelText.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/LevelText.hpp"

#include <cmath>
#include <limits>

TEST(Unit_Dsp, LevelTextUnityPeakReadsZeroDecibels)
{
    EXPECT_NEAR(ui::peak_to_decibels(1.0f), 0.0f, 1e-4f);
}

TEST(Unit_Dsp, LevelTextHalfPeakReadsMinusSixDecibels)
{
    EXPECT_NEAR(ui::peak_to_decibels(0.5f), -6.0206f, 1e-3f);
}

TEST(Unit_Dsp, LevelTextZeroPeakReadsNegativeInfinity)
{
    EXPECT_TRUE(std::isinf(ui::peak_to_decibels(0.0f)));
    EXPECT_LT(ui::peak_to_decibels(0.0f), 0.0f);
}

TEST(Unit_Dsp, LevelTextFillClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(0.0f, -60.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(10.0f, -60.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-60.0f, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-100.0f, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::decibels_to_fill(-30.0f, -60.0f), 0.5f);
}

TEST(Unit_Dsp, LevelTextDecibelLabelFormatsOneDecimalOrNegativeInfinity)
{
    EXPECT_EQ(ui::decibel_label(0.0f), "0.0 dB");
    EXPECT_EQ(ui::decibel_label(-12.34f), "-12.3 dB");
    EXPECT_EQ(ui::decibel_label(-std::numeric_limits<float>::infinity()), "-inf dB");
}
