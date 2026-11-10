/**************************************************************************/
/* test_smoothed_parameter.cpp                                            */
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
 * @file test_smoothed_parameter.cpp
 * @brief Tests the time constant and snapping of SmoothedParameter.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/smoothed_parameter.hpp>

namespace
{
    const double kU = TestSupport::unit_roundoff<float>();

    /// The sample rate of the 20 ms smoother, in hertz, and its time constant in samples.
    constexpr double K_RATE = 48000.0;
    constexpr double K_TIME_CONSTANT_MS = 20.0;
    constexpr std::uint32_t K_TIME_CONSTANT_SAMPLES = 960;

    /// @brief Returns a smoother prepared at the rate with the 20 ms constant and aimed at the target.
    SushiDSP::SmoothedParameter prepared(double sample_rate, float target)
    {
        SushiDSP::SmoothedParameter parameter(0.0f);
        parameter.prepare(sample_rate, static_cast<float>(K_TIME_CONSTANT_MS));
        parameter.set_target(target);
        return parameter;
    }

    /// @brief Calls next the given number of times and returns the last value.
    float advance(SushiDSP::SmoothedParameter& parameter, std::uint32_t steps)
    {
        float value = 0.0f;
        for (std::uint32_t i = 0; i < steps; ++i)
        {
            value = parameter.next();
        }
        return value;
    }

    /// @brief Returns the bound on the distance of the value after steps calls to next from the exact
    /// one-pole response, for a step of at most 1 and a time constant of S samples. The coefficient
    /// is rounded to float, which moves c^n by at most n u c^n; each step rounds the difference, the
    /// product and the sum, 3 u of a quantity of at most 1, and the recursion keeps a fraction c of
    /// that error each step, so the rounding sums to at most 3 u / (1 - c) = 3 u S.
    double next_bound(std::uint32_t steps, double time_constant_samples)
    {
        const double decay = std::exp(-static_cast<double>(steps) / time_constant_samples);
        return kU * (3.0 * time_constant_samples + static_cast<double>(steps) * decay);
    }

    /// @brief Returns the bound on the distance of skip(steps) from the exact response of a step of
    /// at most 1: the float coefficient (n u c^n), the float power (1 u of c^n, taken as 2 u), and the
    /// two roundings of the final difference and product, 2 u.
    double skip_bound(std::uint32_t steps, double time_constant_samples)
    {
        const double decay = std::exp(-static_cast<double>(steps) / time_constant_samples);
        return kU * (static_cast<double>(steps) * decay + 4.0);
    }
} // namespace

TEST(Small_SmoothedParameter, ReachesSixtyThreePercentAtOneTimeConstant)
{
    SushiDSP::SmoothedParameter parameter = prepared(K_RATE, 1.0f);

    const float value = advance(parameter, K_TIME_CONSTANT_SAMPLES);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        value, 1.0 - std::exp(-1.0), next_bound(K_TIME_CONSTANT_SAMPLES, K_TIME_CONSTANT_SAMPLES), 1.0)));
}

TEST(Small_SmoothedParameter, SkipMatchesRepeatedNext)
{
    constexpr std::uint32_t steps = 4096;
    SushiDSP::SmoothedParameter by_next = prepared(K_RATE, 0.7f);
    SushiDSP::SmoothedParameter by_skip = prepared(K_RATE, 0.7f);

    const float via_next = advance(by_next, steps);
    const float via_skip = by_skip.skip(steps);

    const double bound = 0.7 * (next_bound(steps, K_TIME_CONSTANT_SAMPLES) + skip_bound(steps, K_TIME_CONSTANT_SAMPLES));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(via_next, via_skip, bound, 1.0)));
}

TEST(Small_SmoothedParameter, TimeConstantIsSampleRateIndependent)
{
    SushiDSP::SmoothedParameter at_48k = prepared(48000.0, 1.0f);
    SushiDSP::SmoothedParameter at_96k = prepared(96000.0, 1.0f);

    const float value_48k = at_48k.skip(960);
    const float value_96k = at_96k.skip(1920);

    const double bound = skip_bound(960, 960.0) + skip_bound(1920, 1920.0);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(value_48k, value_96k, bound, 1.0)));
}

TEST(Small_SmoothedParameter, SnapSettlesImmediately)
{
    SushiDSP::SmoothedParameter parameter(0.0f);
    parameter.prepare(48000.0, 20.0f);
    parameter.snap(0.3f);
    EXPECT_TRUE(parameter.is_settled());
    EXPECT_FLOAT_EQ(parameter.next(), 0.3f);
}
