/**************************************************************************/
/* test_diffuse_field.cpp                                                 */
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
 * @file test_diffuse_field.cpp
 * @brief Tests that the diffuse field of a room has a non-negative power at the band centres and between them.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/diffuse_field.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

#include <cmath>
#include <cstddef>
#include <string>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::DiffuseField;
    using SushiDSP::room::K_BAND_CENTRES_HZ;

    /// @brief Counts the double roundings of the interpolation at a band centre: the position
    /// divides two logarithms, then one difference, one product and one sum.
    constexpr double K_INTERPOLATION_OPERATIONS = 6.0;

    /// @brief Holds one frequency.
    struct FrequencyRow
    {
            const char* name;
            double frequency_hz;
    };

    /// @brief Holds one interval of frequencies.
    struct IntervalRow
    {
            const char* name;
            double lower_hz;
            double upper_hz;
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

    class Small_DiffuseFieldFrequency : public ::testing::TestWithParam<FrequencyRow>
    {
    };

    class Small_DiffuseFieldInterval : public ::testing::TestWithParam<IntervalRow>
    {
    };

    class Small_DiffuseFieldBand : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST_P(Small_DiffuseFieldBand, DiffuseFieldBandPowerIsThePowerAtTheBandCentre)
{
    const std::size_t band = GetParam().band;
    const DiffuseField field(SushiDSP::test::garage_scene());
    const double previous = field.band_power()[band == 0u ? 0u : band - 1u];
    const double current = field.band_power()[band];

    /* At a centre the interpolation reads the band's own value, to a few roundings of the two
       values it blends. */
    const double bound = K_INTERPOLATION_OPERATIONS * TestSupport::unit_roundoff<double>() *
                         (std::fabs(previous) + std::fabs(current));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(field.power_at(K_BAND_CENTRES_HZ[band]), current, bound)));
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_DiffuseFieldBand,
                         ::testing::Values(BandRow{"Hz125", 0u}, BandRow{"Hz250", 1u},
                                           BandRow{"Hz500", 2u}, BandRow{"Hz1000", 3u},
                                           BandRow{"Hz2000", 4u}, BandRow{"Hz4000", 5u}),
                         row_name<BandRow>);

TEST_P(Small_DiffuseFieldFrequency, DiffuseFieldPowerAtAFrequencyIsNonNegative)
{
    const DiffuseField field(SushiDSP::test::garage_scene());

    EXPECT_GE(field.power_at(GetParam().frequency_hz), 0.0);
}

INSTANTIATE_TEST_SUITE_P(Frequencies, Small_DiffuseFieldFrequency,
                         ::testing::Values(FrequencyRow{"Hz10", 10.0}, FrequencyRow{"Hz125", 125.0},
                                           FrequencyRow{"Hz177", 177.0},
                                           FrequencyRow{"Hz1000", 1000.0},
                                           FrequencyRow{"Hz2800", 2800.0},
                                           FrequencyRow{"Hz20000", 20000.0}),
                         row_name<FrequencyRow>);

TEST_P(Small_DiffuseFieldInterval, DiffuseFieldPowerBetweenTwoFrequenciesIsNonNegative)
{
    const IntervalRow row = GetParam();
    const DiffuseField field(SushiDSP::test::garage_scene());

    EXPECT_GE(field.power_between(row.lower_hz, row.upper_hz), 0.0);
}

INSTANTIATE_TEST_SUITE_P(Intervals, Small_DiffuseFieldInterval,
                         ::testing::Values(IntervalRow{"BelowTheLowestBand", 0.0, 125.0},
                                           IntervalRow{"AllBands", 125.0, 4000.0},
                                           IntervalRow{"AcrossOneEdge", 177.0, 354.0},
                                           IntervalRow{"AboveTheHighestBand", 4000.0, 8000.0},
                                           IntervalRow{"Empty", 1000.0, 1000.0}),
                         row_name<IntervalRow>);

TEST(Small_DiffuseField, WiderBandCarriesAtLeastAsMuchPower)
{
    const DiffuseField field(SushiDSP::test::garage_scene());
    EXPECT_GE(field.power_between(125.0, 8000.0), field.power_between(125.0, 1000.0));
}
