/**************************************************************************/
/* test_knob_math.cpp                                                     */
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
 * @file test_knob_math.cpp
 * @brief Tests the mapping between parameter values and knob positions in KnobMath.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/knob_math.hpp"

#include <cmath>

namespace
{
    constexpr SushiDSP::ParameterDescriptor K_LINEAR{
        "drive", "Drive", 0.0f, 1.0f, 0.5f, SushiDSP::ParamKind::Linear, ""};
    constexpr SushiDSP::ParameterDescriptor K_DECIBEL{
        "master", "Master", -60.0f, 12.0f, 0.0f, SushiDSP::ParamKind::Decibel, "dB"};
    constexpr SushiDSP::ParameterDescriptor K_LOG{
        "freq", "Freq", 20.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz"};
    constexpr SushiDSP::ParameterDescriptor K_LOG_BAD_MIN{
        "freq", "Freq", 0.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz"};
} // namespace

TEST(KnobMath, KnobMathMapsLinearParameterEndpointsAndMidpoint)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 0.5f), 0.5f);
}

TEST(KnobMath, KnobMathMapsDecibelParameterLinearlyInItsOwnUnit)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, 12.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, -24.0f), 0.5f);
}

TEST(KnobMath, KnobMathClampsValuesOutsideTheParameterRange)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, -3.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 4.0f), 1.0f);
}

TEST(KnobMath, KnobMathRoundTripsValueThroughNormalized)
{
    for (float value : {-60.0f, -33.0f, 0.0f, 11.5f})
    {
        const float normalized = ui::value_to_normalized(K_DECIBEL, value);
        EXPECT_NEAR(ui::normalized_to_value(K_DECIBEL, normalized), value, 1e-4f);
    }
}

TEST(KnobMath, KnobMathGivesLogarithmicParameterAGeometricMidpoint)
{
    EXPECT_NEAR(ui::normalized_to_value(K_LOG, 0.5f), std::sqrt(20.0f * 20000.0f), 1e-2f);
    EXPECT_NEAR(ui::value_to_normalized(K_LOG, 20000.0f), 1.0f, 1e-5f);
}

TEST(KnobMath, KnobMathFallsBackToLinearWhenALogarithmicRangeStartsAtZero)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LOG_BAD_MIN, 10000.0f), 0.5f);
    EXPECT_FLOAT_EQ(ui::normalized_to_value(K_LOG_BAD_MIN, 0.5f), 10000.0f);
}

TEST(KnobMath, KnobMathDragMovesUpwardPointerTravelTowardTheMaximum)
{
    // ImGui's y grows downward, so an upward drag is a negative delta.
    EXPECT_GT(ui::apply_drag(0.5f, -20.0f, false), 0.5f);
    EXPECT_LT(ui::apply_drag(0.5f, 20.0f, false), 0.5f);
}

TEST(KnobMath, KnobMathDragClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::apply_drag(0.9f, -1000.0f, false), 1.0f);
    EXPECT_FLOAT_EQ(ui::apply_drag(0.1f, 1000.0f, false), 0.0f);
}

TEST(KnobMath, KnobMathFineDragTravelsExactlyOneQuarterAsFar)
{
    const float coarse = ui::apply_drag(0.0f, -50.0f, false);
    const float fine = ui::apply_drag(0.0f, -50.0f, true);
    EXPECT_NEAR(fine * 4.0f, coarse, 1e-6f);
}

TEST(KnobMath, KnobMathPointsDownLeftAtZeroStraightUpAtHalfAndDownRightAtOne)
{
    // ImGui's y grows downward, so a positive sine points down the screen.
    constexpr float K_DIAGONAL = 0.70710678f;

    const float low = ui::normalized_to_angle(0.0f);
    EXPECT_NEAR(std::cos(low), -K_DIAGONAL, 1e-4f);
    EXPECT_NEAR(std::sin(low), K_DIAGONAL, 1e-4f);

    const float middle = ui::normalized_to_angle(0.5f);
    EXPECT_NEAR(std::cos(middle), 0.0f, 1e-4f);
    EXPECT_NEAR(std::sin(middle), -1.0f, 1e-4f);

    const float high = ui::normalized_to_angle(1.0f);
    EXPECT_NEAR(std::cos(high), K_DIAGONAL, 1e-4f);
    EXPECT_NEAR(std::sin(high), K_DIAGONAL, 1e-4f);
}
