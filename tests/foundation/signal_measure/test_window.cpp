/**************************************************************************/
/* test_window.cpp                                                        */
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
 * @file test_window.cpp
 * @brief Tests that the Blackman-Harris window is symmetric, near zero at its ends and unity at its centre.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/window.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "support/tolerance.hpp"

namespace
{
    /// @brief The window at its ends: 0.35875 - 0.48829 + 0.14128 - 0.01168, every cosine at one.
    constexpr double K_END_VALUE = 0.00006;

    /// @brief The cancelling sum of the four coefficients, in double: four rounded operations on terms
    /// no larger than 0.49.
    constexpr double K_COEFFICIENT_SUM_ERROR = 4.0 * TestSupport::unit_roundoff<double>() * 0.49;

    /// @brief The odd length whose centre sample sits at x = pi, where the cosines alternate.
    constexpr std::uint32_t K_ODD_LENGTH = 513;

    /// @brief The bound on an end sample: the float rounding of its value and the sum's error.
    constexpr double K_END_BOUND = TestSupport::unit_roundoff<float>() * K_END_VALUE + K_COEFFICIENT_SUM_ERROR;

    /// @brief The bound on the mirror difference: the double rounding of x may land a sample and its
    /// mirror on neighbouring floats, one float spacing of a value at most one, 2 u.
    constexpr double K_MIRROR_BOUND = 2.0 * TestSupport::unit_roundoff<float>();

    /// @brief The bound on the centre: the coefficients sum to 1 at x = pi, so the centre is 1 by
    /// construction, off by the float rounding and the sum's error.
    constexpr double K_CENTRE_BOUND = TestSupport::unit_roundoff<float>() + K_COEFFICIENT_SUM_ERROR;

    /// @brief Returns the largest difference between a sample and its mirror image.
    double worst_asymmetry(const std::vector<float>& window)
    {
        double worst = 0.0;
        for (std::size_t i = 0; i < window.size() / 2; ++i)
        {
            worst = std::max(worst, std::fabs(static_cast<double>(window[i]) - window[window.size() - 1 - i]));
        }
        return worst;
    }
} // namespace

TEST(Small_Window, HasTheRequestedSize)
{
    EXPECT_EQ(SushiDSP::measure::blackman_harris(1024).size(), 1024u);
}

TEST(Small_Window, IsNearZeroAtItsFirstEnd)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(SushiDSP::measure::blackman_harris(1024).front(), K_END_VALUE, K_END_BOUND)));
}

TEST(Small_Window, IsNearZeroAtItsLastEnd)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(SushiDSP::measure::blackman_harris(1024).back(), K_END_VALUE, K_END_BOUND)));
}

TEST(Small_Window, IsSymmetricAboutItsCentre)
{
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        worst_asymmetry(SushiDSP::measure::blackman_harris(K_ODD_LENGTH)), 0.0, K_MIRROR_BOUND)));
}

TEST(Small_Window, PeaksAtUnityInTheMiddle)
{
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        SushiDSP::measure::blackman_harris(K_ODD_LENGTH)[K_ODD_LENGTH / 2], 1.0, K_CENTRE_BOUND)));
}
