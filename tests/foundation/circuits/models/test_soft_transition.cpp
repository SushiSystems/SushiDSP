/**************************************************************************/
/* test_soft_transition.cpp                                               */
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
 * @file test_soft_transition.cpp
 * @brief Tests the softplus and logistic functions the device models share.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/models/soft_transition.hpp>

#include <cmath>
#include <string>

namespace
{
    /// @brief Holds one argument of the transition functions and the name its row is reported under.
    struct Argument
    {
            const char* name;
            double t;
    };

    /// @brief Holds one logistic row: the argument and the value 1 / (1 + exp(-t)) is held to.
    struct LogisticRow
    {
            const char* name;
            double t;
            double expected;
    };

    // At t = 0 the logistic is 1/2; at 50 the exponential 1.9e-22 is lost in 1 + e^-t, so the value is 1; at -50 the value is e^t, since 1 + e^t is 1.
    const LogisticRow kLogisticRows[] = {
        {"Zero", 0.0, 0.5},
        {"PlusFifty", 50.0, 1.0},
        {"MinusFifty", -50.0, std::exp(-50.0)},
    };

    const Argument kJointArguments[] = {
        {"MinusFour", -4.0}, {"MinusHalf", -0.5}, {"Zero", 0.0}, {"TwoAndAHalf", 2.5}, {"ThirtyFive", 35.0},
    };
} // namespace

// softplus(0) = log1p(exp(0)) = ln 2: the exponential is exact, so one rounded log1p and one rounded reference log.
TEST(Small_SoftTransition, SoftplusOfZeroIsLnTwo)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(SushiDSP::models::softplus(0.0), std::log(2.0), 2)));
}

TEST(Small_SoftTransition, SoftplusOfFortyIsFortyInDouble)
{
    EXPECT_EQ(SushiDSP::models::softplus(40.0), 40.0);
}

class Small_SoftTransitionLogistic : public ::testing::TestWithParam<LogisticRow>
{
};

// The path is exp, one addition and one division: three rounded operations.
TEST_P(Small_SoftTransitionLogistic, LogisticTakesTheValueOfItsRow)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(SushiDSP::models::logistic(GetParam().t), GetParam().expected, 3)));
}

INSTANTIATE_TEST_SUITE_P(Arguments, Small_SoftTransitionLogistic, ::testing::ValuesIn(kLogisticRows),
                         [](const ::testing::TestParamInfo<LogisticRow>& info) { return std::string(info.param.name); });

// The centred difference errs by h^2/6 times the third derivative sigma(1 - sigma)(1 - 2 sigma), plus the rounding of the two softplus values over 2h.
TEST(Small_SoftTransition, LogisticIsTheDerivativeOfSoftplus)
{
    const double t = 1.3;
    const double h = 1e-5;
    const double logistic = SushiDSP::models::logistic(t);
    const double third_derivative = logistic * (1.0 - logistic) * (1.0 - 2.0 * logistic);
    const double truncation = h * h / 6.0 * std::fabs(third_derivative);
    const double rounding = 2.0 * TestSupport::unit_roundoff<double>() * SushiDSP::models::softplus(t) / (2.0 * h);
    const double slope = (SushiDSP::models::softplus(t + h) - SushiDSP::models::softplus(t - h)) / (2.0 * h);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(slope, logistic, truncation + rounding)));
}

class Small_SoftTransitionJointForm : public ::testing::TestWithParam<Argument>
{
};

// The joint form runs the same expressions as the separate functions, so each value is identical.
TEST_P(Small_SoftTransitionJointForm, AgreesWithTheSeparateFunctions)
{
    double softplus_value = 0.0;
    double logistic_value = 0.0;
    SushiDSP::models::softplus_and_logistic(GetParam().t, softplus_value, logistic_value);
    EXPECT_EQ(softplus_value, SushiDSP::models::softplus(GetParam().t));
    EXPECT_EQ(logistic_value, SushiDSP::models::logistic(GetParam().t));
}

INSTANTIATE_TEST_SUITE_P(Arguments, Small_SoftTransitionJointForm, ::testing::ValuesIn(kJointArguments),
                         [](const ::testing::TestParamInfo<Argument>& info) { return std::string(info.param.name); });
