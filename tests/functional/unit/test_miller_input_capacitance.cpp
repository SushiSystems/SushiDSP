/**************************************************************************/
/* test_miller_input_capacitance.cpp                                      */
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
 * @file test_miller_input_capacitance.cpp
 * @brief The 12AX7 grid capacitance preset's figures and the pole it makes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/MillerInputCapacitance.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double corner_by_hand(double cgk_pf, double cgp_pf, double stray_pf, double gain, double ohms)
    {
        const double farads = (cgk_pf + cgp_pf * (1.0 + gain) + stray_pf) * 1e-12;
        return 1.0 / (2.0 * kPi * ohms * farads);
    }
}

TEST(Unit_Dsp, MillerInputCapacitanceCarriesThe12AX7ModelsOwnFigures)
{
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7MillerCapacitance.grid_cathode_picofarads, 1.6f);
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7MillerCapacitance.grid_plate_picofarads, 1.7f);
    EXPECT_FLOAT_EQ(SushiDSP::kTriode12AX7MillerCapacitance.stray_wiring_picofarads, 10.0f);
}

TEST(Unit_Dsp, MillerInputCapacitanceCornerMatchesTheHandComputedPole)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::kTriode12AX7MillerCapacitance;
    EXPECT_DOUBLE_EQ(c.corner_hz(55.432497, 100000.0),
                     corner_by_hand(1.6f, 1.7f, 10.0f, 55.432497, 100000.0));
    EXPECT_DOUBLE_EQ(c.corner_hz(7.774305, 100000.0),
                     corner_by_hand(1.6f, 1.7f, 10.0f, 7.774305, 100000.0));
}

TEST(Unit_Dsp, MillerInputCapacitanceCornerFallsWithGainAndWithDrivingResistance)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::kTriode12AX7MillerCapacitance;
    EXPECT_LT(c.corner_hz(70.0, 100000.0), c.corner_hz(10.0, 100000.0));
    EXPECT_LT(c.corner_hz(70.0, 220000.0), c.corner_hz(70.0, 100000.0));
    EXPECT_TRUE(std::isfinite(c.corner_hz(0.0, 100000.0)));
}

TEST(Unit_Dsp, MillerInputCapacitanceAtZeroGainIsTheUnmultipliedSum)
{
    const SushiDSP::MillerInputCapacitance c{ 2.0f, 3.0f, 5.0f };
    EXPECT_DOUBLE_EQ(c.corner_hz(0.0, 100000.0), corner_by_hand(2.0, 3.0, 5.0, 0.0, 100000.0));
}
