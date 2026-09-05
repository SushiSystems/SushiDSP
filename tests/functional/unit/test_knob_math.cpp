/**************************************************************************/
/* test_knob_math.cpp                                                     */
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

TEST(Unit_Dsp, KnobMathAngleSpansTheFullSweepAcrossTheNormalizedRange)
{
    EXPECT_FLOAT_EQ(ui::normalized_to_angle(0.0f), ui::kKnobStartRadians);
    EXPECT_FLOAT_EQ(ui::normalized_to_angle(1.0f), ui::kKnobStartRadians + ui::kKnobSweepRadians);
}
