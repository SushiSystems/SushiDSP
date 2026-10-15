/**************************************************************************/
/* test_miller_input_capacitance.cpp                                      */
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
 * @file test_miller_input_capacitance.cpp
 * @brief The 12AX7 grid capacitance preset's figures and the pole it makes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/miller_input_capacitance.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    double corner_by_hand(double cgk_pf, double cgp_pf, double stray_pf, double gain, double ohms)
    {
        const double farads = (cgk_pf + cgp_pf * (1.0 + gain) + stray_pf) * 1e-12;
        return 1.0 / (2.0 * K_PI * ohms * farads);
    }
} // namespace

TEST(Small_MillerInputCapacitance, MillerInputCapacitanceCarriesThe12AX7ModelsOwnFigures)
{
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE.grid_cathode_picofarads, 1.6f);
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE.grid_plate_picofarads, 1.7f);
    EXPECT_FLOAT_EQ(SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE.stray_wiring_picofarads, 10.0f);
}

TEST(Small_MillerInputCapacitance, MillerInputCapacitanceCornerMatchesTheHandComputedPole)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE;
    EXPECT_DOUBLE_EQ(c.corner_hz(55.432497, 100000.0),
                     corner_by_hand(1.6f, 1.7f, 10.0f, 55.432497, 100000.0));
    EXPECT_DOUBLE_EQ(c.corner_hz(7.774305, 100000.0),
                     corner_by_hand(1.6f, 1.7f, 10.0f, 7.774305, 100000.0));
}

TEST(Small_MillerInputCapacitance, MillerInputCapacitanceCornerFallsWithGainAndWithDrivingResistance)
{
    const SushiDSP::MillerInputCapacitance& c = SushiDSP::K_TRIODE12_AX7_MILLER_CAPACITANCE;
    EXPECT_LT(c.corner_hz(70.0, 100000.0), c.corner_hz(10.0, 100000.0));
    EXPECT_LT(c.corner_hz(70.0, 220000.0), c.corner_hz(70.0, 100000.0));
    EXPECT_TRUE(std::isfinite(c.corner_hz(0.0, 100000.0)));
}

TEST(Small_MillerInputCapacitance, MillerInputCapacitanceAtZeroGainIsTheUnmultipliedSum)
{
    const SushiDSP::MillerInputCapacitance c{2.0f, 3.0f, 5.0f};
    EXPECT_DOUBLE_EQ(c.corner_hz(0.0, 100000.0), corner_by_hand(2.0, 3.0, 5.0, 0.0, 100000.0));
}
