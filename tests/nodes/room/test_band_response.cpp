/**************************************************************************/
/* test_band_response.cpp                                                 */
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
 * @file test_band_response.cpp
 * @brief Tests for BandResponse, the six-band gain vector of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <SushiDSP/nodes/room/band_response.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::BandResponse;
    using Bands = std::array<float, SushiDSP::room::K_BAND_COUNT>;

    /// @brief Holds the decibels-per-neper factor of a gain error: 20 / ln 10.
    constexpr double K_DECIBELS_PER_LOG_UNIT = 8.685889638065035;

    /// @brief Bounds the speed of sound's distance from the closed form, in metres per second:
    /// the module holds the figure rounded to a whole metre per second, which moves it by at most
    /// half of one.
    constexpr double K_SPEED_ROUNDING_BOUND = 0.5;

    /// @brief Holds one band index.
    struct BandRow
    {
            const char* name;
            std::size_t band;
    };

    /// @brief Holds one level in decibels with the linear gain it names.
    struct LevelRow
    {
            const char* name;
            float decibels;
            double gain;
    };

    /// @brief Holds a response with the power mean it must report.
    struct PowerMeanRow
    {
            const char* name;
            Bands gain;
            double power_mean;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Expects every band of @p response to equal the matching entry of @p expected.
    void expect_gains_equal(const BandResponse& response, const Bands& expected)
    {
        for (std::size_t band = 0; band < expected.size(); ++band)
        {
            EXPECT_EQ(response.gain[band], expected[band]) << "band " << band;
        }
    }

    /// @brief Expects @p computed within @p operations float roundings of @p reference.
    void expect_float_close(float computed, double reference, std::size_t operations)
    {
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_relative_error<float>(computed, reference, operations)));
    }

    class Small_BandResponsePower : public ::testing::TestWithParam<BandRow>
    {
    };

    class Small_BandResponseDecibels : public ::testing::TestWithParam<LevelRow>
    {
    };

    class Small_BandResponsePowerMean : public ::testing::TestWithParam<PowerMeanRow>
    {
    };

    class Small_BandResponseCentres : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST(Small_BandResponse, BandResponseUnityIsOneInEveryBand)
{
    const BandResponse unity = BandResponse::unity();

    expect_gains_equal(unity, {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f});
}

TEST(Small_BandResponse, BandResponseMultipliesBandByBand)
{
    BandResponse first{{0.5f, 0.5f, 0.25f, 1.0f, 2.0f, 4.0f}};
    const BandResponse second{{2.0f, 4.0f, 4.0f, 1.0f, 0.5f, 0.25f}};

    first *= second;

    expect_gains_equal(first, {1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f});
}

TEST_P(Small_BandResponsePower, BandResponsePowerOfFourIsFourMultiplications)
{
    const BandResponse one{{0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f}};
    const std::size_t band = GetParam().band;
    const double gain = one.gain[band];

    const BandResponse four = one.powered(4.0);

    // The product of four doubles built from an exact float is correct to double rounding.
    expect_float_close(four.gain[band], gain * gain * gain * gain, 1u);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BandResponsePower,
                         ::testing::Values(BandRow{"Band0", 0u}, BandRow{"Band1", 1u},
                                           BandRow{"Band2", 2u}, BandRow{"Band3", 3u},
                                           BandRow{"Band4", 4u}, BandRow{"Band5", 5u}),
                         row_name<BandRow>);

TEST_P(Small_BandResponseDecibels, BandResponseFromDecibelsGivesTheLinearPressureGain)
{
    const LevelRow row = GetParam();
    const BandResponse response = BandResponse::from_decibels({row.decibels, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

    expect_float_close(response.gain[0], row.gain, 1u);
}

TEST_P(Small_BandResponseDecibels, BandResponseDecibelsRoundTrip)
{
    const LevelRow row = GetParam();
    const BandResponse response = BandResponse::from_decibels({row.decibels, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});

    // Rounding the gain to float moves the level by 20 / ln 10 u; the level rounds to float once more.
    const double bound = (K_DECIBELS_PER_LOG_UNIT + std::fabs(row.decibels)) * TestSupport::unit_roundoff<float>();
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(response.decibels()[0], row.decibels, bound)));
}

/*
 * The gains are exact decimal powers of ten, 10^(dB / 20): -20 dB is 0.1, 20 dB is 10 and
 * -40 dB is 0.01.
 */
INSTANTIATE_TEST_SUITE_P(Levels, Small_BandResponseDecibels,
                         ::testing::Values(LevelRow{"Minus40", -40.0f, 0.01},
                                           LevelRow{"Minus20", -20.0f, 0.1},
                                           LevelRow{"Zero", 0.0f, 1.0},
                                           LevelRow{"Plus20", 20.0f, 10.0}),
                         row_name<LevelRow>);

TEST_P(Small_BandResponsePowerMean, BandResponsePowerMeanIsTheRootMeanSquare)
{
    const PowerMeanRow row = GetParam();
    const BandResponse response{row.gain};

    expect_float_close(response.power_mean(), row.power_mean, 1u);
}

/*
 * Six bands of one give sqrt(6 / 6) = 1. Three bands of two and three of zero give
 * sqrt(3 * 4 / 6) = sqrt(2).
 */
INSTANTIATE_TEST_SUITE_P(Responses, Small_BandResponsePowerMean,
                         ::testing::Values(PowerMeanRow{"Flat", {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}, 1.0},
                                           PowerMeanRow{"HalfLoud",
                                                        {0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f},
                                                        1.4142135623730951}),
                         row_name<PowerMeanRow>);

TEST(Small_BandResponse, BandResponseZeroGainReadsAsTheDecibelFloor)
{
    const BandResponse silent{{0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}};

    EXPECT_EQ(silent.decibels()[0], -200.0f);
}

TEST(Small_BandResponse, BandResponseUnityGainReadsAsZeroDecibels)
{
    const BandResponse unity = BandResponse::unity();

    EXPECT_EQ(unity.decibels()[3], 0.0f);
}

TEST_P(Small_BandResponseCentres, BandCentreIsTwiceTheOneBelow)
{
    const std::size_t band = GetParam().band;

    EXPECT_EQ(SushiDSP::room::K_BAND_CENTRES_HZ[band], 2.0 * SushiDSP::room::K_BAND_CENTRES_HZ[band - 1u]);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_BandResponseCentres,
                         ::testing::Values(BandRow{"Band1", 1u}, BandRow{"Band2", 2u},
                                           BandRow{"Band3", 3u}, BandRow{"Band4", 4u},
                                           BandRow{"Band5", 5u}),
                         row_name<BandRow>);

TEST(Small_BandResponse, BandCentresStartAtOneTwentyFiveHertz)
{
    EXPECT_EQ(SushiDSP::room::K_BAND_CENTRES_HZ[0], 125.0);
}

TEST(Small_BandResponse, SpeedOfSoundIsTheClosedFormAtTwentyDegreesToAWholeMetrePerSecond)
{
    // 331.3 m/s at 0 degrees C, scaled by sqrt(T / 273.15) at 293.15 K, is 343.2 m/s.
    const double closed_form = 331.3 * std::sqrt(293.15 / 273.15);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(SushiDSP::room::K_SPEED_OF_SOUND, closed_form, K_SPEED_ROUNDING_BOUND)));
}
