/**************************************************************************/
/* test_calibration.cpp                                                   */
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
 * @file test_calibration.cpp
 * @brief Tests the decibel and gain conversions and the full-scale reference.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/calibration.hpp>

#include <cmath>

TEST(Calibration, FullScaleIsTwoVolts)
{
    EXPECT_FLOAT_EQ(SushiDSP::K_FULL_SCALE_VOLTS, 2.0f);
}

TEST(Calibration, ZeroDecibelsIsUnityGain)
{
    EXPECT_NEAR(SushiDSP::decibels_to_gain(0.0f), 1.0f, 1e-6f);
}

TEST(Calibration, TwentyDecibelsIsTenTimesTheGain)
{
    EXPECT_NEAR(SushiDSP::decibels_to_gain(20.0f), 10.0f, 1e-4f);
    EXPECT_NEAR(SushiDSP::decibels_to_gain(-20.0f), 0.1f, 1e-6f);
}

TEST(Calibration, GainAndDecibelsConvertBothWays)
{
    for (float db : {-60.0f, -6.0f, 0.0f, 3.0f, 12.0f})
    {
        EXPECT_NEAR(SushiDSP::gain_to_decibels(SushiDSP::decibels_to_gain(db)), db, 1e-3f);
    }
}

TEST(Calibration, ZeroGainIsClampedInsteadOfBecomingMinusInfinity)
{
    const float db = SushiDSP::gain_to_decibels(0.0f);
    EXPECT_TRUE(std::isfinite(db));
    EXPECT_NEAR(db, 20.0f * std::log10(1e-9f), 1e-3f);
}
