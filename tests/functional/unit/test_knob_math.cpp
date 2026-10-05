/****************************************************************/
/* test_knob_math.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_knob_math.cpp
 * @brief Tests the mapping between parameter values and knob positions in KnobMath.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/KnobMath.hpp"

#include <cmath>

namespace
{
    constexpr SushiDSP::ParameterDescriptor kLinear{
        "drive", "Drive", 0.0f, 1.0f, 0.5f, SushiDSP::ParamKind::Linear, "" };
    constexpr SushiDSP::ParameterDescriptor kDecibel{
        "master", "Master", -60.0f, 12.0f, 0.0f, SushiDSP::ParamKind::Decibel, "dB" };
    constexpr SushiDSP::ParameterDescriptor kLog{
        "freq", "Freq", 20.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz" };
    constexpr SushiDSP::ParameterDescriptor kLogBadMin{
        "freq", "Freq", 0.0f, 20000.0f, 1000.0f, SushiDSP::ParamKind::Logarithmic, "Hz" };
}

TEST(Unit_Dsp, KnobMathMapsLinearParameterEndpointsAndMidpoint)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 0.5f), 0.5f);
}

TEST(Unit_Dsp, KnobMathMapsDecibelParameterLinearlyInItsOwnUnit)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, 12.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kDecibel, -24.0f), 0.5f);
}

TEST(Unit_Dsp, KnobMathClampsValuesOutsideTheParameterRange)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, -3.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLinear, 4.0f), 1.0f);
}

TEST(Unit_Dsp, KnobMathRoundTripsValueThroughNormalized)
{
    for (float value : { -60.0f, -33.0f, 0.0f, 11.5f })
    {
        const float normalized = ui::value_to_normalized(kDecibel, value);
        EXPECT_NEAR(ui::normalized_to_value(kDecibel, normalized), value, 1e-4f);
    }
}

TEST(Unit_Dsp, KnobMathGivesLogarithmicParameterAGeometricMidpoint)
{
    EXPECT_NEAR(ui::normalized_to_value(kLog, 0.5f), std::sqrt(20.0f * 20000.0f), 1e-2f);
    EXPECT_NEAR(ui::value_to_normalized(kLog, 20000.0f), 1.0f, 1e-5f);
}

TEST(Unit_Dsp, KnobMathFallsBackToLinearWhenALogarithmicRangeStartsAtZero)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(kLogBadMin, 10000.0f), 0.5f);
    EXPECT_FLOAT_EQ(ui::normalized_to_value(kLogBadMin, 0.5f), 10000.0f);
}

TEST(Unit_Dsp, KnobMathDragMovesUpwardPointerTravelTowardTheMaximum)
{
    // ImGui's y grows downward, so an upward drag is a negative delta.
    EXPECT_GT(ui::apply_drag(0.5f, -20.0f, false), 0.5f);
    EXPECT_LT(ui::apply_drag(0.5f, 20.0f, false), 0.5f);
}

TEST(Unit_Dsp, KnobMathDragClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::apply_drag(0.9f, -1000.0f, false), 1.0f);
    EXPECT_FLOAT_EQ(ui::apply_drag(0.1f, 1000.0f, false), 0.0f);
}

TEST(Unit_Dsp, KnobMathFineDragTravelsExactlyOneQuarterAsFar)
{
    const float coarse = ui::apply_drag(0.0f, -50.0f, false);
    const float fine = ui::apply_drag(0.0f, -50.0f, true);
    EXPECT_NEAR(fine * 4.0f, coarse, 1e-6f);
}

TEST(Unit_Dsp, KnobMathPointsDownLeftAtZeroStraightUpAtHalfAndDownRightAtOne)
{
    // ImGui's y grows downward, so a positive sine points down the screen.
    constexpr float kDiagonal = 0.70710678f;

    const float low = ui::normalized_to_angle(0.0f);
    EXPECT_NEAR(std::cos(low), -kDiagonal, 1e-4f);
    EXPECT_NEAR(std::sin(low), kDiagonal, 1e-4f);

    const float middle = ui::normalized_to_angle(0.5f);
    EXPECT_NEAR(std::cos(middle), 0.0f, 1e-4f);
    EXPECT_NEAR(std::sin(middle), -1.0f, 1e-4f);

    const float high = ui::normalized_to_angle(1.0f);
    EXPECT_NEAR(std::cos(high), kDiagonal, 1e-4f);
    EXPECT_NEAR(std::sin(high), kDiagonal, 1e-4f);
}
