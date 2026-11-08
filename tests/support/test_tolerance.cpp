/**************************************************************************/
/* test_tolerance.cpp                                                     */
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
 * @file test_tolerance.cpp
 * @brief Tests that the copied tolerance helper keeps its promises under this repository's compiler flags.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <limits>

namespace
{

/**
 * @brief One measurement and whether the helper must accept it.
 */
struct WithinRow
{
    /** @brief Names the row in the test output. */
    char const* name;
    /** @brief The value the code under test produced. */
    double computed;
    /** @brief The value the oracle gives. */
    double reference;
    /** @brief The rounding bound. */
    double bound;
    /** @brief Whether the measurement passes. */
    bool within;
};

/**
 * @brief The rows of the acceptance rule: exact results, NaN and infinity on either side.
 */
WithinRow const K_WITHIN_ROWS[] = {
    {"AnExactResultPassesAZeroBound", 1.0, 1.0, 0.0, true},
    {"AnInexactResultFailsAZeroBound", 1.0, 1.0 + 0x1p-52, 0.0, false},
    {"ANanWhereANumberIsExpectedFails", std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0, false},
    {"ANumberWhereANanIsExpectedFails", 1.0, std::numeric_limits<double>::quiet_NaN(), 1.0, false},
    {"ANanWhereANanIsExpectedPasses", std::numeric_limits<double>::quiet_NaN(),
     std::numeric_limits<double>::quiet_NaN(), 0.0, true},
    {"AMatchingInfinityPasses", std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), 0.0,
     true},
    {"AnInfinityWhereANumberIsExpectedFails", std::numeric_limits<double>::infinity(), 1.0, 1.0, false},
};

class Small_ToleranceWithin : public ::testing::TestWithParam<WithinRow>
{
};

} // namespace

TEST_P(Small_ToleranceWithin, AcceptsExactlyTheMeasurementsTheRuleAllows)
{
    WithinRow const& row = GetParam();
    EXPECT_EQ(TestSupport::measure_error(row.computed, row.reference, row.bound).is_within(), row.within);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_ToleranceWithin, ::testing::ValuesIn(K_WITHIN_ROWS),
                         [](::testing::TestParamInfo<WithinRow> const& info) { return info.param.name; });

TEST(Small_Tolerance, UnitRoundoffOfFloatIsTwoToTheMinusTwentyFour)
{
    EXPECT_EQ(TestSupport::unit_roundoff<float>(), 0x1p-24);
}

TEST(Small_Tolerance, UnitRoundoffOfDoubleIsTwoToTheMinusFiftyThree)
{
    EXPECT_EQ(TestSupport::unit_roundoff<double>(), 0x1p-53);
}

TEST(Small_Tolerance, SumErrorAtTheThresholdPasses)
{
    double const u = TestSupport::unit_roundoff<float>();
    EXPECT_TRUE(TestSupport::measure_sum_error<float>(1.0 + 16.0 * 4.0 * u, 1.0, 1.0, 4).is_within());
}

TEST(Small_Tolerance, SumErrorPastTheThresholdFails)
{
    double const u = TestSupport::unit_roundoff<float>();
    EXPECT_FALSE(TestSupport::measure_sum_error<float>(1.0 + 17.0 * 4.0 * u, 1.0, 1.0, 4).is_within());
}

TEST(Small_Tolerance, SumRatioIsErrorOverLengthTimesRoundoffTimesMagnitude)
{
    double const u = TestSupport::unit_roundoff<float>();
    EXPECT_EQ(TestSupport::measure_sum_error<float>(1.0 + 4.0 * u, 1.0, 1.0, 4).ratio, 1.0);
}

TEST(Small_Tolerance, RelativeBoundScalesWithTheReference)
{
    EXPECT_EQ(TestSupport::measure_relative_error<double>(2.0 + 0x1p-51, 2.0, 2).ratio, 1.0);
}

TEST(Small_Tolerance, DescribeNamesErrorBoundRatioAndThreshold)
{
    EXPECT_EQ(TestSupport::measure_error(1.5, 1.0, 1.0, 0.25).describe(), "error 0.5, bound 1, ratio 0.5 of 0.25");
}
