/**************************************************************************/
/* test_resonance_control.cpp                                             */
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
 * @file test_resonance_control.cpp
 * @brief Tests the depth and boost of ResonanceControl.
 * @author Mustafa Garip
 */

#include <cmath>
#include <complex>
#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/stages/resonance_control.hpp>
#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr double K_CORNER_HZ = 120.0;
    constexpr double K_MAXIMUM_BOOST_DB = 9.0;
    constexpr double K_SHELF_Q = 0.7071067811865476;

    /// @brief Holds the relative error allowed on the measured amplitude: the sine in float, the filter in float over a second, and the sum of squares, counted as a thousand roundings.
    const double kAmplitudeError = 1000.0 * TestSupport::unit_roundoff<float>();

    /// @brief Returns the steady gain of a sine through the control, in decibels, over a whole number of cycles.
    ///
    /// The settle and the measuring window are one second each, an integer number of cycles of the frequencies the rows use.
    float steady_gain_db(SushiDSP::ResonanceControl& control, double frequency_hz)
    {
        const int settle = 48000;
        const int measure = 48000;
        double sum_squares_out = 0.0;
        double sum_squares_in = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            const float x = static_cast<float>(std::sin(2.0 * K_PI * frequency_hz * i / K_SAMPLE_RATE));
            const float y = control.process_sample(x);
            if (i >= settle)
            {
                sum_squares_out += y * y;
                sum_squares_in += x * x;
            }
        }
        return static_cast<float>(10.0 * std::log10(sum_squares_out / sum_squares_in));
    }

    /// @brief Returns the magnitude in decibels of the Audio EQ Cookbook low shelf at @p frequency_hz.
    double cookbook_low_shelf_db(double gain_db, double frequency_hz)
    {
        const double a = std::pow(10.0, gain_db / 40.0);
        const double w0 = 2.0 * K_PI * K_CORNER_HZ / K_SAMPLE_RATE;
        const double alpha = std::sin(w0) / (2.0 * K_SHELF_Q);
        const double c = std::cos(w0);
        const double s = 2.0 * std::sqrt(a) * alpha;
        const double b0 = a * ((a + 1.0) - (a - 1.0) * c + s);
        const double b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * c);
        const double b2 = a * ((a + 1.0) - (a - 1.0) * c - s);
        const double a0 = (a + 1.0) + (a - 1.0) * c + s;
        const double a1 = -2.0 * ((a - 1.0) + (a + 1.0) * c);
        const double a2 = (a + 1.0) + (a - 1.0) * c - s;
        const std::complex<double> z1 = std::polar(1.0, -2.0 * K_PI * frequency_hz / K_SAMPLE_RATE);
        const std::complex<double> z2 = z1 * z1;
        const double magnitude = std::abs((b0 + b1 * z1 + b2 * z2) / (a0 + a1 * z1 + a2 * z2));
        return 20.0 * std::log10(magnitude);
    }

    /// @brief Holds one depth, one frequency and the name the row is reported under.
    struct ShelfRow
    {
            const char* name;
            float depth;
            double frequency_hz;
    };

    const ShelfRow kShelfRows[] = {
        {"ZeroDepthLow", 0.0f, 40.0},
        {"ZeroDepthHigh", 0.0f, 4000.0},
        {"FullDepthLow", 1.0f, 20.0},
        {"FullDepthHigh", 1.0f, 4000.0},
    };
} // namespace

class Small_ResonanceControlShelf : public ::testing::TestWithParam<ShelfRow>
{
};

// The control is a low shelf of depth times the maximum boost; the cookbook response is the oracle, and the amplitude error converts to decibels as 20 / ln 10 times the relative error.
TEST_P(Small_ResonanceControlShelf, GainMatchesTheCookbookLowShelf)
{
    SushiDSP::ResonanceControl control(K_SAMPLE_RATE, static_cast<float>(K_CORNER_HZ), static_cast<float>(K_MAXIMUM_BOOST_DB));
    const ShelfRow& row = GetParam();
    control.set_depth(row.depth);
    const double expected = cookbook_low_shelf_db(static_cast<double>(row.depth) * K_MAXIMUM_BOOST_DB, row.frequency_hz);
    const double bound = 20.0 / std::log(10.0) * kAmplitudeError;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(steady_gain_db(control, row.frequency_hz), expected, bound)));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_ResonanceControlShelf, ::testing::ValuesIn(kShelfRows),
                         [](const ::testing::TestParamInfo<ShelfRow>& info) { return std::string(info.param.name); });

TEST(Small_ResonanceControl, ResonanceControlClampsDepthAndReportsBoost)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(2.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 9.0f);
    control.set_depth(-1.0f);
    EXPECT_FLOAT_EQ(control.boost_db(), 0.0f);
    control.set_depth(0.5f);
    EXPECT_FLOAT_EQ(control.boost_db(), 4.5f);
}

namespace
{
    /// @brief Runs 512 samples of a ramp through @p unit, the way an audio block drives it.
    template <typename Unit> void process_ramp_block(Unit& unit)
    {
        for (int sample = 0; sample < 512; ++sample)
        {
            static_cast<void>(unit.process_sample(static_cast<float>(sample % 64) / 64.0f - 0.5f));
        }
    }
} // namespace

TEST(Medium_ResonanceControl, ProcessSamplePerformsNoHeapAllocation)
{
    SushiDSP::ResonanceControl control(48000.0, 120.0f, 9.0f);
    control.set_depth(0.7f);
    process_ramp_block(control);
    SushiDSP::test::AllocationTrap trap;

    trap.arm();
    process_ramp_block(control);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
