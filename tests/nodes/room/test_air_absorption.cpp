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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
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

TEST(Unit_Dsp, AirAbsorptionMatchesIso9613AtTwentyDegreesAndFiftyPercent)
{
    const std::array<double, 6> expected = {0.00044, 0.00131, 0.00273, 0.00466, 0.00989, 0.02967};
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const double computed = SushiDSP::room::AirAbsorption::attenuation_db_per_metre(
            SushiDSP::room::K_BAND_CENTRES_HZ[band]);
        EXPECT_NEAR(computed, expected[band], 0.05 * expected[band]) << "band " << band;
    }
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::attenuation_db_per_metre(8000.0), 0.1053, 0.006);
}

TEST(Unit_Dsp, AirAbsorptionEnergyCoefficientIsTheDecibelFigureOverFourPointThreeFourThree)
{
    const double four_kilohertz = SushiDSP::room::AirAbsorption::attenuation_db_per_metre(4000.0);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(5u),
                four_kilohertz / (10.0 * std::log10(std::exp(1.0))), 1e-9);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(5u), 6.831e-3, 3.5e-4);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(0u), 1.01e-4, 5.1e-6);
}

TEST(Unit_Dsp, AirAbsorptionOverThirtyMetresCostsUnderOneDecibel)
{
    const SushiDSP::room::BandResponse thirty = SushiDSP::room::AirAbsorption::over_path(30.0);
    EXPECT_NEAR(thirty.decibels()[5], -0.89f, 0.05f); // 30 * 0.02967
    EXPECT_GT(thirty.decibels()[0], -0.02f);
}

TEST(Unit_Dsp, AirAbsorptionOverZeroMetresIsUnity)
{
    const SushiDSP::room::BandResponse none = SushiDSP::room::AirAbsorption::over_path(0.0);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(none.gain[band], 1.0f);
    }
}

TEST(Unit_Dsp, AirAbsorptionGrowsWithFrequencyAndDistance)
{
    const SushiDSP::room::BandResponse ten = SushiDSP::room::AirAbsorption::over_path(10.0);
    const SushiDSP::room::BandResponse twenty = SushiDSP::room::AirAbsorption::over_path(20.0);
    for (std::size_t band = 1; band < 6u; ++band)
    {
        EXPECT_LE(ten.gain[band], ten.gain[band - 1u]);
        EXPECT_LT(twenty.gain[band], ten.gain[band]);
    }
}
