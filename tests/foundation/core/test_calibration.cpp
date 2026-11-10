/**************************************************************************/
/* test_calibration.cpp                                                   */
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
 * @file test_calibration.cpp
 * @brief Tests the decibel and gain conversions.
 * @author Mustafa Garip
 */

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/core/calibration.hpp>

namespace
{
    const double kU = TestSupport::unit_roundoff<float>();

    /// @brief Holds a level in decibels, its name and the gain it stands for.
    struct LevelRow
    {
            const char* name;
            float decibels;
            double gain;
    };

    const LevelRow kSpecifiedLevels[] = {
        {"ZeroDecibelsIsUnity", 0.0f, 1.0},
        {"PlusTwentyIsTen", 20.0f, 10.0},
        {"MinusTwentyIsATenth", -20.0f, 0.1},
    };

    const LevelRow kRoundTripLevels[] = {
        {"MinusSixty", -60.0f, 0.0}, {"MinusSix", -6.0f, 0.0}, {"Zero", 0.0f, 0.0}, {"PlusThree", 3.0f, 0.0}, {"PlusTwelve", 12.0f, 0.0},
    };

    // The division db / 20 rounds once and the power multiplies that relative error by ln 10 times the exponent, then pow rounds within two units: (2 + 2.31 |db| / 20) u relative, plus one more for the product.
    double gain_relative_bound(float decibels)
    {
        return (3.0 + 2.31 * std::fabs(static_cast<double>(decibels)) / 20.0) * kU;
    }

    std::string level_name(const ::testing::TestParamInfo<LevelRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_CalibrationLevels : public ::testing::TestWithParam<LevelRow>
{
};

TEST_P(Small_CalibrationLevels, DecibelsToGainGivesTheSpecifiedGain)
{
    const LevelRow& row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        SushiDSP::decibels_to_gain(row.decibels), row.gain, gain_relative_bound(row.decibels) * row.gain)));
}

INSTANTIATE_TEST_SUITE_P(Specified, Small_CalibrationLevels, ::testing::ValuesIn(kSpecifiedLevels), level_name);

class Small_CalibrationRoundTrip : public ::testing::TestWithParam<LevelRow>
{
};

TEST_P(Small_CalibrationRoundTrip, GainAndDecibelsConvertBothWays)
{
    const float decibels = GetParam().decibels;
    // The gain carries gain_relative_bound; 20 log10 turns a relative error into 20 / ln 10 of decibels; the log and the product round once more each.
    const double bound = 8.69 * gain_relative_bound(decibels) + 3.0 * kU * (std::fabs(decibels) + 1.0);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        SushiDSP::gain_to_decibels(SushiDSP::decibels_to_gain(decibels)), decibels, bound)));
}

INSTANTIATE_TEST_SUITE_P(Levels, Small_CalibrationRoundTrip, ::testing::ValuesIn(kRoundTripLevels), level_name);

TEST(Small_Calibration, ZeroGainIsClampedToAFiniteLevel)
{
    EXPECT_TRUE(std::isfinite(SushiDSP::gain_to_decibels(0.0f)));
}

TEST(Small_Calibration, ZeroGainReadsTheOneNanogainFloor)
{
    // The header floors the gain at 1e-9, which is 20 log10(1e-9) = -180 dB. The float 1e-9 carries u relative error, which is 8.69 u in decibels, and the log and the product round on top of a result of 180.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(SushiDSP::gain_to_decibels(0.0f), -180.0, 8.69 * kU + 3.0 * kU * 180.0)));
}
