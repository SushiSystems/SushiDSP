/**************************************************************************/
/* test_plate_swing.cpp                                                   */
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
 * @file test_plate_swing.cpp
 * @brief Tests that the plate output stays inside its headroom and that its antiderivative matches it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>

#include <cmath>
#include <string>

namespace
{
    constexpr double K_SUPPLY_HEADROOM = 100.0;
    constexpr double K_BOTTOM_HEADROOM = 50.0;

    const SushiDSP::PlateSwing K_SWING{static_cast<float>(K_SUPPLY_HEADROOM), static_cast<float>(K_BOTTOM_HEADROOM)};

    const double kU = TestSupport::unit_roundoff<double>();

    /// @brief Holds one saturation row: a linear voltage far past a headroom, the headroom's sign and magnitude.
    struct SaturationRow
    {
            const char* name;
            double linear_volts;
            double headroom;
    };

    const SaturationRow kSaturationRows[] = {
        {"TowardTheSupply", 1000.0, K_SUPPLY_HEADROOM},
        {"TowardTheBottom", -1000.0, -K_BOTTOM_HEADROOM},
    };

    /// @brief Holds one voltage for the slope rows.
    struct Voltage
    {
            const char* name;
            double volts;
    };

    const Voltage kSlopeVoltages[] = {
        {"MinusOneHundredTwenty", -120.0}, {"MinusTwenty", -20.0}, {"Zero", 0.0}, {"Twenty", 20.0}, {"OneHundredTwenty", 120.0},
    };

    // tanh(z) is 1 - 2 exp(-2 z) to first order, so at z = x / H the bound sits 2 H exp(-2 |x| / H) inside its headroom; a tanh and a product round on top.
    double saturation_bound(const SaturationRow& row)
    {
        const double headroom = std::fabs(row.headroom);
        return 2.0 * headroom * std::exp(-2.0 * std::fabs(row.linear_volts) / headroom) + 4.0 * kU * headroom;
    }

    /// @brief Returns the headroom of the side a linear voltage swings toward, as the swing selects it.
    double side_headroom(double x)
    {
        return x > 0.0 ? K_SUPPLY_HEADROOM : K_BOTTOM_HEADROOM;
    }

    // The antiderivative is H^2 (z + log1p(exp(-2 z)) - ln 2) with z = |x| / H. Near zero the three terms cancel to z^2 / 2 while each carries a rounding of its own size, so the error follows H^2 (z + ln 2), not the value: the quotient z rounds once, the exponential adds at most (1 + 2 z) u / 2 through log1p, log1p rounds at ln 2 u, the sum rounds at (z + ln 2) u, the literal ln 2 and the final subtraction round at ln 2 u each, so the total stays under 4 (z + 1) u H^2.
    double antiderivative_rounding(double x)
    {
        const double headroom = side_headroom(x);
        return 4.0 * kU * headroom * headroom * (std::fabs(x) / headroom + 1.0);
    }

    // A centred difference has truncation h^2 / 6 of the third derivative of the antiderivative, here the slope of the bound, at most 0.77 / H_min. The rounding is that of the two antiderivative values over 2h; the two points x +- h are rounded to the grid of x, which moves the width 2h by at most 2 u |x|, a relative u |x| / h of the slope; and the bound itself rounds twice.
    double slope_bound(double x, double step)
    {
        const double truncation = step * step / 6.0 * 0.77 / K_BOTTOM_HEADROOM;
        const double rounding = (antiderivative_rounding(x + step) + antiderivative_rounding(x - step)) / (2.0 * step);
        const double width = kU * std::fabs(x) / step * std::fabs(K_SWING.bound(x));
        return truncation + rounding + width + 2.0 * kU * std::fabs(K_SWING.bound(x));
    }
} // namespace

// tanh(0) is exactly zero, so the bound is zero in both precisions.
TEST(Small_PlateSwing, ZeroLinearVoltageGivesZeroPlate)
{
    EXPECT_EQ(K_SWING.bound(0.0), 0.0);
    EXPECT_EQ(K_SWING.bound(0.0f), 0.0f);
}

class Small_PlateSwingSaturation : public ::testing::TestWithParam<SaturationRow>
{
};

TEST_P(Small_PlateSwingSaturation, LargeExcursionsSaturateAtTheirOwnHeadroom)
{
    const SaturationRow& row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(K_SWING.bound(row.linear_volts), row.headroom, saturation_bound(row))));
}

INSTANTIATE_TEST_SUITE_P(Sides, Small_PlateSwingSaturation, ::testing::ValuesIn(kSaturationRows),
                         [](const ::testing::TestParamInfo<SaturationRow>& info) { return std::string(info.param.name); });

TEST(Small_PlateSwing, OutputIsMonotonicAcrossZero)
{
    EXPECT_LT(K_SWING.bound(-2.0), K_SWING.bound(-1.0));
    EXPECT_LT(K_SWING.bound(-1.0), K_SWING.bound(0.5));
    EXPECT_LT(K_SWING.bound(0.5), K_SWING.bound(1.0));
}

class Small_PlateSwingAntiderivative : public ::testing::TestWithParam<Voltage>
{
};

TEST_P(Small_PlateSwingAntiderivative, IsTheIntegralOfTheBound)
{
    constexpr double step = 1e-4;
    const double x = GetParam().volts;
    const double slope = (K_SWING.bound_antiderivative(x + step) - K_SWING.bound_antiderivative(x - step)) / (2.0 * step);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(slope, K_SWING.bound(x), slope_bound(x, step))));
}

INSTANTIATE_TEST_SUITE_P(Points, Small_PlateSwingAntiderivative, ::testing::ValuesIn(kSlopeVoltages),
                         [](const ::testing::TestParamInfo<Voltage>& info) { return std::string(info.param.name); });
