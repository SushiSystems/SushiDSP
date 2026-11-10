/**************************************************************************/
/* test_air_absorption.cpp                                                */
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
 * @file test_air_absorption.cpp
 * @brief Tests for AirAbsorption, the ISO 9613-1 attenuation of the room module.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include <SushiDSP/nodes/room/air_absorption.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::AirAbsorption;
    using SushiDSP::room::BandResponse;

    /// @brief Characterised band: the ISO 9613-1 table is printed to three digits and the
    /// module's own fit follows the standard's closed form, so a reading may sit this far,
    /// relative to the table value, from the printed figure. Kept from the original test.
    constexpr double K_TABLE_RELATIVE_BAND = 0.05;

    /// @brief Characterised band of the 8 kHz row, in decibels per metre. Kept from the
    /// original test.
    constexpr double K_EIGHT_KILOHERTZ_BAND = 0.006;

    /// @brief Gives the decibels-per-neper factor, 10 log10(e), from its definition.
    constexpr double K_DECIBELS_PER_NEPER = 4.342944819032518;

    /// @brief Holds one tabulated attenuation of ISO 9613-1 at 20 degrees C and 50 percent.
    struct AttenuationRow
    {
            const char* name;
            double frequency_hz;
            double db_per_metre;
            double band;
    };

    /// @brief Holds one band of the energy coefficient with its tabulated decibel figure.
    struct CoefficientRow
    {
            const char* name;
            std::size_t band;
            double db_per_metre;
    };

    /// @brief Holds the index of one band.
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

    /// @brief Expects @p computed to be @p reference within @p band.
    void expect_within(double computed, double reference, double band)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, band, 1.0)));
    }

    class Small_AirAbsorptionTable : public ::testing::TestWithParam<AttenuationRow>
    {
    };

    class Small_AirAbsorptionCoefficient : public ::testing::TestWithParam<CoefficientRow>
    {
    };

    class Small_AirAbsorptionBand : public ::testing::TestWithParam<BandRow>
    {
    };
} // namespace

TEST_P(Small_AirAbsorptionTable, AirAbsorptionMatchesIso9613AtTwentyDegreesAndFiftyPercent)
{
    const AttenuationRow row = GetParam();

    expect_within(AirAbsorption::attenuation_db_per_metre(row.frequency_hz), row.db_per_metre, row.band);
}

INSTANTIATE_TEST_SUITE_P(
    Bands, Small_AirAbsorptionTable,
    ::testing::Values(
        AttenuationRow{"Hz125", 125.0, 0.00044, K_TABLE_RELATIVE_BAND * 0.00044},
        AttenuationRow{"Hz250", 250.0, 0.00131, K_TABLE_RELATIVE_BAND * 0.00131},
        AttenuationRow{"Hz500", 500.0, 0.00273, K_TABLE_RELATIVE_BAND * 0.00273},
        AttenuationRow{"Hz1000", 1000.0, 0.00466, K_TABLE_RELATIVE_BAND * 0.00466},
        AttenuationRow{"Hz2000", 2000.0, 0.00989, K_TABLE_RELATIVE_BAND * 0.00989},
        AttenuationRow{"Hz4000", 4000.0, 0.02967, K_TABLE_RELATIVE_BAND * 0.02967},
        AttenuationRow{"Hz8000", 8000.0, 0.1053, K_EIGHT_KILOHERTZ_BAND}),
    row_name<AttenuationRow>);

TEST_P(Small_AirAbsorptionCoefficient, AirAbsorptionEnergyCoefficientIsTheTabulatedDecibelFigureOverFourPointThreeFourThree)
{
    const CoefficientRow row = GetParam();
    const double reference = row.db_per_metre / K_DECIBELS_PER_NEPER;

    expect_within(AirAbsorption::energy_coefficient_per_metre(row.band), reference,
                  K_TABLE_RELATIVE_BAND * reference);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_AirAbsorptionCoefficient,
                         ::testing::Values(CoefficientRow{"Hz125", 0u, 0.00044},
                                           CoefficientRow{"Hz4000", 5u, 0.02967}),
                         row_name<CoefficientRow>);

TEST(Small_AirAbsorption, AirAbsorptionOverThirtyMetresAtFourKilohertzCostsUnderOneDecibel)
{
    const BandResponse thirty = AirAbsorption::over_path(30.0);

    // Thirty metres at 0.02967 dB per metre is 0.8901 dB.
    const double reference = -30.0 * 0.02967;
    expect_within(thirty.decibels()[5], reference, K_TABLE_RELATIVE_BAND * -reference);
}

TEST(Small_AirAbsorption, AirAbsorptionOverThirtyMetresAtOneTwentyFiveHertzCostsUnderTwoHundredthsOfADecibel)
{
    const BandResponse thirty = AirAbsorption::over_path(30.0);

    // Thirty metres at 0.00044 dB per metre is 0.0132 dB.
    const double reference = -30.0 * 0.00044;
    expect_within(thirty.decibels()[0], reference, K_TABLE_RELATIVE_BAND * -reference);
}

TEST(Small_AirAbsorption, AirAbsorptionOverZeroMetresIsUnity)
{
    const BandResponse none = AirAbsorption::over_path(0.0);

    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(none.gain[band], 1.0f);
    }
}

TEST_P(Small_AirAbsorptionBand, AirAbsorptionGrowsWithFrequency)
{
    const std::size_t band = GetParam().band;
    const BandResponse ten = AirAbsorption::over_path(10.0);

    EXPECT_LE(ten.gain[band], ten.gain[band - 1u]);
}

INSTANTIATE_TEST_SUITE_P(Bands, Small_AirAbsorptionBand,
                         ::testing::Values(BandRow{"Hz250", 1u}, BandRow{"Hz500", 2u},
                                           BandRow{"Hz1000", 3u}, BandRow{"Hz2000", 4u},
                                           BandRow{"Hz4000", 5u}),
                         row_name<BandRow>);

TEST_P(Small_AirAbsorptionBand, AirAbsorptionGrowsWithDistance)
{
    const std::size_t band = GetParam().band;
    const BandResponse ten = AirAbsorption::over_path(10.0);
    const BandResponse twenty = AirAbsorption::over_path(20.0);

    EXPECT_LT(twenty.gain[band], ten.gain[band]);
}
