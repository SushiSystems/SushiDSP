/**************************************************************************/
/* test_cabinet_directivity.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
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

#include "SushiDSP/room/CabinetDirectivity.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;
}

TEST(Unit_Dsp, CabinetDirectivityIsUnityOnAxis)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse on_axis = cabinet.gain_for(0.0, 0.0);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_NEAR(on_axis.gain[band], 1.0f, 1e-6f);
    }
}

TEST(Unit_Dsp, CabinetDirectivityBeamsAtTwoKilohertzAndNotAtOneTwentyFive)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse off = cabinet.gain_for(kPi / 3.0, 0.0);   // 60 degrees
    // 2 kHz: a(f) = 0.125 / sqrt(1 + 0.64) = 0.097614 m, k = 36.6300 /m,
    // k*a*sin60 = 3.0958 so the piston is 0.195099 (-14.19 dB), and the array is
    // |cos(36.6300 * 0.30 * 0.866025 / 2)| = 0.046834 (-26.59 dB).
    EXPECT_NEAR(off.decibels()[4], -40.78f, 0.10f);
    // 125 Hz: piston 0.992358, array 0.956086, together -0.46 dB.
    EXPECT_NEAR(off.decibels()[0], -0.46f, 0.10f);
}

TEST(Unit_Dsp, CabinetDirectivityPistonFactorMatchesTheBesselSeries)
{
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(0.0), 1.0, 1e-12);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(1.0), 0.880101, 1e-6);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(3.831706), 0.0, 1e-4);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(-1.0), 0.880101, 1e-6);
}

TEST(Unit_Dsp, CabinetDirectivityIndexIsSmallWhereTheConeIsOmnidirectional)
{
    // 125 Hz gives k*a = 0.286, so the piston and array together cost under 0.2 dB over the
    // sphere; the closed box's rear shelf reads -1.55 dB over the back hemisphere, which is
    // 10*log10(2 / (1 + 0.70)) = 0.71 dB of index on its own. Together the index stays under
    // 1.5 dB, and by 2 kHz the beam is over 10 dB.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const std::array<float, 6> index = cabinet.directivity_index_db();
    EXPECT_GT(index[0], 0.5f);
    EXPECT_LT(index[0], 1.5f);
    EXPECT_GT(index[4], 10.0f);
    for (std::size_t band = 1; band < 6u; ++band)
    {
        EXPECT_GT(index[band], index[band - 1u]);
    }
}

TEST(Unit_Dsp, CabinetDirectivityIsSymmetricLeftAndRight)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    for (const double azimuth : { 0.2, 0.7, 1.4, 2.4 })
    {
        const SushiDSP::room::BandResponse left = cabinet.gain_for(azimuth, 0.0);
        const SushiDSP::room::BandResponse right = cabinet.gain_for(-azimuth, 0.0);
        for (std::size_t band = 0; band < 6u; ++band)
        {
            EXPECT_NEAR(left.gain[band], right.gain[band], 1e-6f) << "azimuth " << azimuth;
        }
    }
}

TEST(Unit_Dsp, CabinetDirectivityFallsSixDecibelsBehindTheBaffleAboveTheStep)
{
    // Straight behind, the piston and the array are both 1 and only the rear shelf remains.
    // The 4x12's step is 115 / 0.75 = 153.3 Hz. At 4 kHz the shelf is
    // sqrt((1 + (4000/306.7)^2) / (1 + (4000/153.3)^2)) = sqrt(171.1 / 681.6) = -6.00 dB;
    // at 125 Hz it is sqrt(1.1662 / 1.6647) = -1.55 dB.
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const std::array<float, 6> behind = cabinet.gain_for(kPi, 0.0).decibels();
    EXPECT_NEAR(behind[5], -6.00f, 0.05f);
    EXPECT_NEAR(behind[0], -1.55f, 0.05f);
    EXPECT_NEAR(behind[5] - behind[0], -4.45f, 0.10f);
}
