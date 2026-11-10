/**************************************************************************/
/* test_cabinet_directivity.cpp                                           */
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
 * @file test_cabinet_directivity.cpp
 * @brief Tests for CabinetDirectivity, the piston-in-a-baffle model of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include <SushiDSP/nodes/room/cabinet_directivity.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::BandResponse;
    using SushiDSP::room::CabinetDirectivity;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Bounds the on-axis gain: the energy mean of sixteen ones is exactly one in
    /// double, and the cast to float rounds once; four roundings leave room for the square root.
    constexpr double K_ON_AXIS_BOUND = 4.0 * TestSupport::unit_roundoff<float>();

    /// @brief Characterised band, in decibels: the model samples sixteen points per octave and
    /// the closed form below 1024, and the two agree to this at every direction tried. Kept
    /// from the original test, as is its use for the printed octave means.
    constexpr double K_OCTAVE_MEAN_BAND_DB = 0.05;

    /// @brief Characterised band, in decibels, of the printed rear-shelf levels. Kept from the
    /// original test.
    constexpr double K_REAR_BAND_DB = 0.05;

    /// @brief Bounds the rounding of the ascending series of 2 J1(x) / x in double: the largest term at x = 10 is
    /// near 565 and each term rounds by u, 565 u is 6e-14.
    constexpr double K_SERIES_BOUND = 1.0e-10;

    /// @brief Holds one argument of the piston factor with the value the Bessel table gives.
    struct PistonRow
    {
            const char* name;
            double x;
            double factor;
            double bound;
    };

    /// @brief Holds one direction with the left and up sines its closed form needs.
    struct DirectionRow
    {
            const char* name;
            double azimuth_rad;
            double elevation_rad;
    };

    /// @brief Holds one azimuth.
    struct AzimuthRow
    {
            const char* name;
            double azimuth_rad;
    };

    /// @brief Holds one band index.
    struct BandRow
    {
            const char* name;
            std::size_t band;
    };

    /// @brief Names a row for gtest.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return info.param.name;
    }

    /// @brief Returns 2 J1(x) / x from its ascending series, 40 terms, for |x| below 12 where
    /// the series converges to double: sum over k of (-1)^k (x / 2)^(2k) / (k! (k + 1)!).
    double series_piston_factor(double x)
    {
        const double half_squared = 0.25 * x * x;
        double term = 1.0;
        double sum = term;
        for (int index = 1; index < 40; ++index)
        {
            term *= -half_squared / (static_cast<double>(index) * static_cast<double>(index + 1));
            sum += term;
        }
        return sum;
    }

    /// @brief Returns the 4x12 closed form at one frequency in the front hemisphere: the piston
    /// factor at the shrinking effective radius times the two-by-two array factor.
    double closed_form(double frequency_hz, double left, double up)
    {
        const SushiDSP::room::CabinetGeometry& g = SushiDSP::room::K_CABINET4X12;
        const double wavenumber = 2.0 * K_PI * frequency_hz / SushiDSP::room::K_SPEED_OF_SOUND;
        const double ratio = frequency_hz / g.breakup_frequency_hz;
        const double radius = g.cone_radius_m / std::sqrt(1.0 + ratio * ratio);
        const double sine = std::sqrt(left * left + up * up);
        const double piston = series_piston_factor(wavenumber * radius * sine);
        const double half_spacing = 0.5 * wavenumber * g.driver_spacing_m;
        return std::fabs(piston) * std::fabs(std::cos(half_spacing * left)) *
               std::fabs(std::cos(half_spacing * up));
    }

    /// @brief Returns the energy mean of the closed form over one octave band, in decibels, by
    /// the midpoint rule over 1024 equal log-frequency intervals.
    double octave_mean_db(std::size_t band, double left, double up)
    {
        constexpr int points = 1024;
        double power_sum = 0.0;
        for (int point = 0; point < points; ++point)
        {
            const double octaves = (static_cast<double>(point) + 0.5) / points - 0.5;
            const double frequency = SushiDSP::room::K_BAND_CENTRES_HZ[band] * std::exp2(octaves);
            const double factor = closed_form(frequency, left, up);
            power_sum += factor * factor;
        }
        return 10.0 * std::log10(power_sum / points);
    }

    /// @brief Returns the cabinet's gain per band for the 4x12 in decibels at a direction.
    std::array<float, SushiDSP::room::K_BAND_COUNT> decibels_at(double azimuth, double elevation)
    {
        const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);
        return cabinet.gain_for(azimuth, elevation).decibels();
    }

    /// @brief Expects @p computed to be @p reference within @p bound, in decibels.
    void expect_decibels_near(double computed, double reference, double bound)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, bound, 1.0)))
            << computed << " against " << reference;
    }

    /// @brief Expects every band of the 4x12's gain at a direction to match the octave mean of
    /// the closed form at that direction.
    void expect_matches_octave_means(double azimuth, double elevation)
    {
        const double left = std::cos(elevation) * std::sin(azimuth);
        const double up = std::sin(elevation);
        const std::array<float, SushiDSP::room::K_BAND_COUNT> got = decibels_at(azimuth, elevation);
        for (std::size_t band = 0; band < got.size(); ++band)
        {
            expect_decibels_near(got[band], octave_mean_db(band, left, up), K_OCTAVE_MEAN_BAND_DB);
        }
    }

    /// @brief Expects the 4x12's gain at @p azimuth to equal its gain at minus @p azimuth, band
    /// by band, to one float rounding.
    void expect_mirror_symmetric(double azimuth)
    {
        const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);
        const BandResponse left = cabinet.gain_for(azimuth, 0.0);
        const BandResponse right = cabinet.gain_for(-azimuth, 0.0);
        for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
        {
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_relative_error<float>(left.gain[band], right.gain[band], 1u)))
                << "band " << band;
        }
    }

    class Small_CabinetDirectivityPiston : public ::testing::TestWithParam<PistonRow>
    {
    };

    class Small_CabinetDirectivityOctaveMean : public ::testing::TestWithParam<DirectionRow>
    {
    };

    class Small_CabinetDirectivitySymmetry : public ::testing::TestWithParam<AzimuthRow>
    {
    };

    class Small_CabinetDirectivityIndex : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST_P(Small_CabinetDirectivityPiston, CabinetDirectivityPistonFactorMatchesTheBesselTable)
{
    const PistonRow row = GetParam();

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(CabinetDirectivity::piston_factor(row.x), row.factor, row.bound)));
}

/*
 * Abramowitz and Stegun table 9.1 gives J1 to ten digits: J1(1) = 0.4400505857, J1(2) =
 * 0.5767248078, J1(5) = -0.3275791376 and J1(10) = 0.04347274617, so 2 J1(x) / x is 0.8801011714,
 * 0.5767248078, -0.1310316550 and 0.008694549234, each good to the table's half digit. The first
 * zero of J1 is 3.8317059702; the argument is given to seven digits, which moves the factor by
 * 3e-8 times 2 J1'(x) / x = 6.3e-9, so that row takes 1e-8.
 */
INSTANTIATE_TEST_SUITE_P(Arguments, Small_CabinetDirectivityPiston,
                         ::testing::Values(PistonRow{"Zero", 0.0, 1.0, 0.0},
                                           PistonRow{"One", 1.0, 0.8801011714, K_SERIES_BOUND},
                                           PistonRow{"Two", 2.0, 0.5767248078, K_SERIES_BOUND},
                                           PistonRow{"FirstZero", 3.831706, 0.0, 1.0e-8},
                                           PistonRow{"Five", 5.0, -0.1310316550, K_SERIES_BOUND},
                                           PistonRow{"Ten", 10.0, 0.008694549234, K_SERIES_BOUND}),
                         row_name<PistonRow>);

TEST(Small_CabinetDirectivity, CabinetDirectivityPistonFactorIsEvenInItsArgument)
{
    EXPECT_EQ(CabinetDirectivity::piston_factor(-1.0), CabinetDirectivity::piston_factor(1.0));
}

TEST(Small_CabinetDirectivity, CabinetDirectivityIsUnityOnAxis)
{
    const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);

    const BandResponse on_axis = cabinet.gain_for(0.0, 0.0);

    for (std::size_t band = 0; band < SushiDSP::room::K_BAND_COUNT; ++band)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(on_axis.gain[band], 1.0, K_ON_AXIS_BOUND)))
            << "band " << band;
    }
}

TEST(Small_CabinetDirectivity, CabinetDirectivityBeamsAtTwoKilohertz)
{
    // The 2 kHz band's energy mean reads -15.29 dB. See tests/nodes/room/README.md.
    expect_decibels_near(decibels_at(K_PI / 3.0, 0.0)[4], -15.29, K_OCTAVE_MEAN_BAND_DB);
}

TEST(Small_CabinetDirectivity, CabinetDirectivityDoesNotBeamAtOneTwentyFive)
{
    // The 125 Hz band's energy mean reads -0.49 dB. See tests/nodes/room/README.md.
    expect_decibels_near(decibels_at(K_PI / 3.0, 0.0)[0], -0.49, K_OCTAVE_MEAN_BAND_DB);
}

TEST(Small_CabinetDirectivity, CabinetDirectivityAveragesTheArrayNullOverTheOctave)
{
    // The row null at 2120 Hz averages to -11.48 dB. See tests/nodes/room/README.md.
    const std::array<float, SushiDSP::room::K_BAND_COUNT> direct = decibels_at(0.0, std::atan2(0.7, 2.5));

    expect_decibels_near(direct[4], -11.48, K_OCTAVE_MEAN_BAND_DB);
    expect_decibels_near(direct[5], -3.49, K_OCTAVE_MEAN_BAND_DB);
}

TEST_P(Small_CabinetDirectivityOctaveMean, CabinetDirectivityMatchesTheOctaveMeanOfItsClosedForm)
{
    // Sixteen points per octave match 1024 within 0.05 dB. See tests/nodes/room/README.md.
    const DirectionRow row = GetParam();

    expect_matches_octave_means(row.azimuth_rad, row.elevation_rad);
}

INSTANTIATE_TEST_SUITE_P(Directions, Small_CabinetDirectivityOctaveMean,
                         ::testing::Values(DirectionRow{"SixtyDegreesOffAxis", K_PI / 3.0, 0.0},
                                           DirectionRow{"DirectPathUp", 0.0, std::atan2(0.7, 2.5)},
                                           DirectionRow{"SteeplyUp", 0.0, std::atan2(3.3, 2.5)}),
                         row_name<DirectionRow>);

TEST(Small_CabinetDirectivity, CabinetDirectivityIndexIsUnderOneAndAHalfDecibelsAtOneTwentyFive)
{
    // The index stays under 1.5 dB at 125 Hz. See tests/nodes/room/README.md.
    const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);

    const float index = cabinet.directivity_index_db()[0];

    EXPECT_GT(index, 0.5f);
    EXPECT_LT(index, 1.5f);
}

TEST(Small_CabinetDirectivity, CabinetDirectivityIndexIsOverTenDecibelsAtTwoKilohertz)
{
    const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);

    EXPECT_GT(cabinet.directivity_index_db()[4], 10.0f);
}

TEST_P(Small_CabinetDirectivityIndex, CabinetDirectivityIndexRisesWithBand)
{
    const std::size_t band = GetParam().band;
    const CabinetDirectivity cabinet(SushiDSP::room::K_CABINET4X12);

    const std::array<float, SushiDSP::room::K_BAND_COUNT> index = cabinet.directivity_index_db();

    EXPECT_GT(index[band], index[band - 1u]);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_CabinetDirectivityIndex,
                         ::testing::Values(BandRow{"Hz250", 1u}, BandRow{"Hz500", 2u},
                                           BandRow{"Hz1000", 3u}, BandRow{"Hz2000", 4u},
                                           BandRow{"Hz4000", 5u}),
                         row_name<BandRow>);

TEST_P(Small_CabinetDirectivitySymmetry, CabinetDirectivityIsSymmetricLeftAndRight)
{
    expect_mirror_symmetric(GetParam().azimuth_rad);
}

INSTANTIATE_TEST_SUITE_P(Azimuths, Small_CabinetDirectivitySymmetry,
                         ::testing::Values(AzimuthRow{"Rad0Point2", 0.2}, AzimuthRow{"Rad0Point7", 0.7},
                                           AzimuthRow{"Rad1Point4", 1.4}, AzimuthRow{"Rad2Point4", 2.4}),
                         row_name<AzimuthRow>);

TEST(Small_CabinetDirectivity, CabinetDirectivityFallsSixDecibelsBehindTheBaffleAtFourKilohertz)
{
    // Straight behind, only the rear shelf remains. See tests/nodes/room/README.md.
    expect_decibels_near(decibels_at(K_PI, 0.0)[5], -6.00, K_REAR_BAND_DB);
}

TEST(Small_CabinetDirectivity, CabinetDirectivityFallsOnePointFiveFiveDecibelsBehindTheBaffleBelowTheStep)
{
    expect_decibels_near(decibels_at(K_PI, 0.0)[0], -1.55, K_REAR_BAND_DB);
}
