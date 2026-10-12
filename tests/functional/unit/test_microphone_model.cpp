/**************************************************************************/
/* test_microphone_model.cpp                                              */
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
 * @file test_microphone_model.cpp
 * @brief Tests for MicrophoneModel, the first-order pattern with its proximity effect.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

#include "SushiDSP/room/microphone_model.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Returns a cardioid's on-axis gain in decibels at one frequency and distance,
    /// 0.5 + 0.5 * B(f, r), the far-field value being 1.
    double cardioid_on_axis_db(double frequency_hz, double distance_m)
    {
        return 20.0 * std::log10(0.5 + 0.5 * SushiDSP::room::MicrophoneModel::proximity_boost(
                                                 frequency_hz, distance_m));
    }
} // namespace

TEST(Unit_Dsp, MicrophoneModelCardioidAtFiveCentimetresGainsFifteenPointFiveDecibels)
{
    // At 100 Hz the two distances differ by 15.47 dB. See tests/functional/unit/README.md.
    EXPECT_NEAR(cardioid_on_axis_db(100.0, 0.05) - cardioid_on_axis_db(100.0, 3.0), 15.47, 0.05);
    // At 125 Hz the two distances differ by 13.75 dB. See tests/functional/unit/README.md.
    const SushiDSP::room::MicrophoneModel cardioid(SushiDSP::room::PolarPattern::Cardioid,
                                                   {0, 0, 0, 0, 0, 0});
    const float near_db = cardioid.gain_for(0.0, 0.05).decibels()[0];
    const float far_db = cardioid.gain_for(0.0, 3.00).decibels()[0];
    EXPECT_NEAR(near_db, 13.80f, 0.05f);
    EXPECT_NEAR(near_db - far_db, 13.75f, 0.05f);
}

TEST(Unit_Dsp, MicrophoneModelProximityBoostIsTheSphericalWaveTerm)
{
    // sqrt(1 + 10.9180^2) = 10.9637 at 100 Hz and 5 cm. See tests/functional/unit/README.md.
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(100.0, 0.05), 10.9637, 1e-4);
    // 343 / (2 pi * 100 * 0.30) = 1.81967, sqrt(1 + 3.31120) = 2.07634.
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(100.0, 0.30), 2.07634, 1e-4);
    // 343 / (2 pi * 1000 * 3) = 0.018197, sqrt(1 + 3.3112e-4) = 1.000166.
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(1000.0, 3.0), 1.000166, 1e-6);
}

TEST(Unit_Dsp, MicrophoneModelCardioidAtThirtyCentimetresGainsThreePointSevenDecibels)
{
    // At 100 Hz and 30 cm the capsule gains 3.74 dB. See tests/functional/unit/README.md.
    EXPECT_NEAR(cardioid_on_axis_db(100.0, 0.30), 3.74, 0.02);
    EXPECT_NEAR(cardioid_on_axis_db(100.0, 0.30) - cardioid_on_axis_db(100.0, 3.0), 3.67, 0.02);
    // At 125 Hz and 30 cm the capsule gains 2.82 dB. See tests/functional/unit/README.md.
    const SushiDSP::room::MicrophoneModel cardioid(SushiDSP::room::PolarPattern::Cardioid,
                                                   {0, 0, 0, 0, 0, 0});
    const float thirty_db = cardioid.gain_for(0.0, 0.30).decibels()[0];
    const float far_db = cardioid.gain_for(0.0, 3.00).decibels()[0];
    EXPECT_NEAR(thirty_db, 2.82f, 0.02f);
    EXPECT_NEAR(thirty_db - far_db, 2.77f, 0.02f);
}

TEST(Unit_Dsp, MicrophoneModelFigureEightRejectsNinetyDegreesByFortyDecibels)
{
    // The pattern term floors at K_DECIBEL_FLOOR, never a NaN. See tests/functional/unit/README.md.
    const SushiDSP::room::MicrophoneModel eight(SushiDSP::room::PolarPattern::FigureEight,
                                                {0, 0, 0, 0, 0, 0});
    const SushiDSP::room::BandResponse side = eight.gain_for(K_PI / 2.0, 1.0);
    const SushiDSP::room::BandResponse front = eight.gain_for(0.0, 1.0);
    const std::array<float, 6> side_db = side.decibels();
    const std::array<float, 6> front_db = front.decibels();
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FALSE(std::isnan(side.gain[band]));
        EXPECT_LT(side.gain[band], 1e-10f);
        EXPECT_FLOAT_EQ(side_db[band], SushiDSP::room::K_DECIBEL_FLOOR);
        EXPECT_GE(front_db[band] - side_db[band], 40.0f);
    }
}

TEST(Unit_Dsp, MicrophoneModelOmniIgnoresIncidenceAndDistanceColour)
{
    const SushiDSP::room::MicrophoneModel omni(SushiDSP::room::PolarPattern::Omni,
                                               {0, 0, 0, 0, 0, 0});
    const SushiDSP::room::BandResponse reference = omni.gain_for(0.0, 0.05);
    for (const double incidence : {0.0, K_PI / 3.0, K_PI})
    {
        for (const double distance : {0.05, 3.0})
        {
            const SushiDSP::room::BandResponse gain = omni.gain_for(incidence, distance);
            for (std::size_t band = 0; band < 6u; ++band)
            {
                EXPECT_FLOAT_EQ(gain.gain[band], reference.gain[band])
                    << "incidence " << incidence << " distance " << distance;
                EXPECT_FLOAT_EQ(gain.gain[band], 1.0f);
            }
        }
    }
}

TEST(Unit_Dsp, MicrophoneModelAppliesTheOnAxisCurve)
{
    const SushiDSP::room::MicrophoneModel flat(SushiDSP::room::PolarPattern::Cardioid,
                                               {0, 0, 0, 0, 0, 0});
    const SushiDSP::room::MicrophoneModel coloured(SushiDSP::room::PolarPattern::Cardioid,
                                                   {0, 0, 0, 0, 3, 6});
    const std::array<float, 6> expected_difference = {0.0f, 0.0f, 0.0f, 0.0f, 3.0f, 6.0f};
    for (const double distance : {0.05, 0.30, 3.0})
    {
        const std::array<float, 6> flat_db = flat.gain_for(0.3, distance).decibels();
        const std::array<float, 6> coloured_db = coloured.gain_for(0.3, distance).decibels();
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(coloured_db[band] - flat_db[band], expected_difference[band], 1e-4f)
                << "distance " << distance << " band " << band;
        }
    }
}

TEST(Unit_Dsp, MicrophoneModelShipsOneFlatPreset)
{
    EXPECT_EQ(SushiDSP::room::K_MICROPHONE_PRESET_COUNT, 1u);
    EXPECT_EQ(SushiDSP::room::K_MICROPHONE_FLAT_CARDIOID.pattern,
              SushiDSP::room::PolarPattern::Cardioid);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(SushiDSP::room::K_MICROPHONE_FLAT_CARDIOID.on_axis_db[band], 0.0f);
    }
    EXPECT_DOUBLE_EQ(SushiDSP::room::pattern_pressure_share(SushiDSP::room::PolarPattern::Omni),
                     1.0);
    EXPECT_DOUBLE_EQ(SushiDSP::room::pattern_pressure_share(SushiDSP::room::PolarPattern::Cardioid),
                     0.5);
    EXPECT_DOUBLE_EQ(
        SushiDSP::room::pattern_pressure_share(SushiDSP::room::PolarPattern::Supercardioid), 0.366);
    EXPECT_DOUBLE_EQ(
        SushiDSP::room::pattern_pressure_share(SushiDSP::room::PolarPattern::FigureEight), 0.0);
}
