/****************************************************************/
/* test_miller_input_capacitance.cpp                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
