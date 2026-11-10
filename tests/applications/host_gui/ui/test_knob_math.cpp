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
 * @file test_knob_math.cpp
 * @brief Tests the mapping between parameter values and knob positions in KnobMath.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <string>

#include "support/tolerance.hpp"
#include "ui/knob_math.hpp"

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

    /// @brief The decibel range's span, 12 dB - (-60 dB).
    constexpr double K_DECIBEL_SPAN = 72.0;

    /// @brief Rounded operations on the longest path of a decibel round trip: the offset, the
    /// division, the multiplication and the addition.
    constexpr std::size_t K_DECIBEL_ROUND_TRIP_OPERATIONS = 4;

    /// @brief Rounded operations on the longest path of the logarithmic midpoint: the ratio's
    /// division, the power, the product, and the square root of the reference.
    constexpr std::size_t K_LOG_MIDPOINT_OPERATIONS = 6;

    /// @brief Rounded operations on the longest path of the fine and coarse drag: the division
    /// and the subtraction, then the reference's multiplication.
    constexpr std::size_t K_DRAG_OPERATIONS = 4;

    /// @brief Roundings on the longest path of a knob angle: the representation of the start and
    /// of the sweep constants, the product and the sum.
    constexpr std::size_t K_ANGLE_OPERATIONS = 4;

    /// @brief The largest angle the knob reaches, in radians: 135 degrees of start and 270 degrees
    /// of sweep.
    constexpr double K_LARGEST_ANGLE_RADIANS = 7.0685834705770345;

    /// @brief The bound, in float, on a sine or cosine of a knob angle.
    ///
    /// The angle carries K_ANGLE_OPERATIONS roundings of at most u times its size, the sine and
    /// the cosine are 1-Lipschitz in their argument, and the function adds one rounding of its own
    /// below 1 in size.
    constexpr double K_TRIG_BOUND =
        (static_cast<double>(K_ANGLE_OPERATIONS) * K_LARGEST_ANGLE_RADIANS + 1.0) *
        TestSupport::unit_roundoff<float>();

    /// @brief Checks the direction the pointer takes at a knob position.
    /// @param normalized The knob position.
    /// @param cosine The cosine the pointer angle should have.
    /// @param sine The sine the pointer angle should have.
    /// @return Success, or the measured error of whichever component missed.
    ::testing::AssertionResult points_along(float normalized, double cosine, double sine)
    {
        const float angle = ui::normalized_to_angle(normalized);
        const TestSupport::ErrorRatio cosine_error =
            TestSupport::measure_error(std::cos(angle), cosine, K_TRIG_BOUND);
        const TestSupport::ErrorRatio sine_error =
            TestSupport::measure_error(std::sin(angle), sine, K_TRIG_BOUND);
        if (!cosine_error.is_within())
        {
            return ::testing::AssertionFailure() << "cosine: " << cosine_error.describe();
        }
        if (!sine_error.is_within())
        {
            return ::testing::AssertionFailure() << "sine: " << sine_error.describe();
        }
        return ::testing::AssertionSuccess();
    }

    /// @brief The cosine and sine of the diagonal directions, 45 degrees from an axis: both are the square root of one half.
    const double K_DIAGONAL = std::sqrt(0.5);
} // namespace

TEST(Small_KnobMath, KnobMathMapsLinearParameterEndpointsAndMidpoint)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 0.5f), 0.5f);
}

TEST(Small_KnobMath, KnobMathMapsDecibelParameterLinearlyInItsOwnUnit)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, -60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, 12.0f), 1.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_DECIBEL, -24.0f), 0.5f);
}

TEST(Small_KnobMath, KnobMathClampsValuesOutsideTheParameterRange)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, -3.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LINEAR, 4.0f), 1.0f);
}

namespace
{
    /// @brief One decibel value that goes out to a knob position and back.
    struct RoundTripRow
    {
            const char* name;
            float value;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string round_trip_row_name(const ::testing::TestParamInfo<RoundTripRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_KnobMathRoundTrip : public ::testing::TestWithParam<RoundTripRow>
{
};

TEST_P(Small_KnobMathRoundTrip, KnobMathRoundTripsValueThroughNormalized)
{
    const float value = GetParam().value;
    const float normalized = ui::value_to_normalized(K_DECIBEL, value);
    const double bound = static_cast<double>(K_DECIBEL_ROUND_TRIP_OPERATIONS) *
                         TestSupport::unit_roundoff<float>() * K_DECIBEL_SPAN;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        ui::normalized_to_value(K_DECIBEL, normalized), value, bound)));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_KnobMathRoundTrip,
                         ::testing::Values(RoundTripRow{"MinusSixty", -60.0f},
                                           RoundTripRow{"MinusThirtyThree", -33.0f},
                                           RoundTripRow{"Zero", 0.0f},
                                           RoundTripRow{"ElevenAndAHalf", 11.5f}),
                         round_trip_row_name);

TEST(Small_KnobMath, KnobMathGivesLogarithmicParameterAGeometricMidpoint)
{
    const double geometric_mean = std::sqrt(20.0 * 20000.0);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        ui::normalized_to_value(K_LOG, 0.5f), geometric_mean, K_LOG_MIDPOINT_OPERATIONS)));
}

TEST(Small_KnobMath, KnobMathMapsTheLogarithmicMaximumToOne)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LOG, 20000.0f), 1.0f);
}

TEST(Small_KnobMath, KnobMathFallsBackToLinearWhenALogarithmicRangeStartsAtZero)
{
    EXPECT_FLOAT_EQ(ui::value_to_normalized(K_LOG_BAD_MIN, 10000.0f), 0.5f);
    EXPECT_FLOAT_EQ(ui::normalized_to_value(K_LOG_BAD_MIN, 0.5f), 10000.0f);
}

TEST(Small_KnobMath, KnobMathDragMovesUpwardPointerTravelTowardTheMaximum)
{
    // ImGui's y grows downward, so an upward drag is a negative delta.
    EXPECT_GT(ui::apply_drag(0.5f, -20.0f, false), 0.5f);
    EXPECT_LT(ui::apply_drag(0.5f, 20.0f, false), 0.5f);
}

TEST(Small_KnobMath, KnobMathDragClampsAtBothEnds)
{
    EXPECT_FLOAT_EQ(ui::apply_drag(0.9f, -1000.0f, false), 1.0f);
    EXPECT_FLOAT_EQ(ui::apply_drag(0.1f, 1000.0f, false), 0.0f);
}

TEST(Small_KnobMath, KnobMathFineDragTravelsExactlyOneQuarterAsFar)
{
    const float coarse = ui::apply_drag(0.0f, -50.0f, false);
    const float fine = ui::apply_drag(0.0f, -50.0f, true);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(fine * 4.0f, coarse, K_DRAG_OPERATIONS)));
}

TEST(Small_KnobMath, KnobMathPointsDownLeftAtZero)
{
    // ImGui's y grows downward, so a positive sine points down the screen.
    EXPECT_TRUE(points_along(0.0f, -K_DIAGONAL, K_DIAGONAL));
}

TEST(Small_KnobMath, KnobMathPointsStraightUpAtHalf)
{
    EXPECT_TRUE(points_along(0.5f, 0.0, -1.0));
}

TEST(Small_KnobMath, KnobMathPointsDownRightAtOne)
{
    EXPECT_TRUE(points_along(1.0f, K_DIAGONAL, K_DIAGONAL));
}
