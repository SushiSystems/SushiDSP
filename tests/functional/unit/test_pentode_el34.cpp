/**************************************************************************/
/* test_pentode_el34.cpp                                                  */
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
 * @file test_pentode_el34.cpp
 * @brief Tests the PentodeEL34 model against hand-computed currents and derivatives.
 * @author Mustafa Garip
 */

#include <array>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/models/pentode_el34.hpp>

TEST(Unit_Dsp, PentodeEL34MatchesKorenAtThreeOperatingPoints)
{
    // The three points are hand-computed from Koren's law. See tests/functional/unit/README.md.
    const SushiDSP::PentodeEL34 pentode;
    struct Point
    {
            double vg1k;
            double vg2k;
            double vpk;
            double ip;
            double dip_dvg1k;
            double dip_dvg2k;
            double dip_dvpk;
            double ig2;
            double dig2_dvg1k;
            double dig2_dvg2k;
    };
    const Point points[3] = {
        {-37.0, 400.0, 400.0, 3.34149261300818e-2, 4.98316907313410e-3, 5.73718514953930e-4,
         3.30555615548759e-6, 0.0, 0.0, 0.0},
        {-20.0, 300.0, 250.0, 7.92947981392609e-2, 1.04281593600714e-2, 1.05203721563143e-3,
         2.04537709631731e-5, 3.23652408855726e-3, 6.00779783938442e-4, 5.46163439944038e-5},
        {-5.0, 400.0, 100.0, 4.31496731894557e-1, 1.83712705886168e-2, 1.68594235250184e-3,
         7.33338162266083e-4, 2.32789367120296e-2, 1.00200640630040e-3, 9.10914914818548e-5},
    };
    for (const Point& point : points)
    {
        const std::array<double, 3> voltages{point.vg1k, point.vg2k, point.vpk};
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current(voltages, currents);
        pentode.jacobian(voltages, jacobian);
        EXPECT_NEAR(currents[2], point.ip, 1e-12 + 1e-9 * point.ip) << point.vg1k;
        EXPECT_NEAR(currents[1], point.ig2, 1e-12 + 1e-9 * point.ig2) << point.vg1k;
        EXPECT_NEAR(jacobian[2][0], point.dip_dvg1k, 1e-12 + 1e-9 * point.dip_dvg1k) << point.vg1k;
        EXPECT_NEAR(jacobian[2][1], point.dip_dvg2k, 1e-12 + 1e-9 * point.dip_dvg2k) << point.vg1k;
        EXPECT_NEAR(jacobian[2][2], point.dip_dvpk, 1e-14 + 1e-9 * point.dip_dvpk) << point.vg1k;
        EXPECT_NEAR(jacobian[1][0], point.dig2_dvg1k, 1e-12 + 1e-9 * point.dig2_dvg1k)
            << point.vg1k;
        EXPECT_NEAR(jacobian[1][1], point.dig2_dvg2k, 1e-13 + 1e-9 * point.dig2_dvg2k)
            << point.vg1k;
        EXPECT_DOUBLE_EQ(jacobian[1][2], 0.0);
        EXPECT_DOUBLE_EQ(jacobian[0][1], 0.0);
        EXPECT_DOUBLE_EQ(jacobian[0][2], 0.0);
    }
}

TEST(Unit_Dsp, PentodeEL34GridCurrentFollowsNormansSoftLaw)
{
    // Ig1 = 1e-6 * ln(1 + exp((Vg1k - 0.5)/0.15)). See tests/functional/unit/README.md.
    const SushiDSP::PentodeEL34 pentode;
    struct Point
    {
            double vg1k;
            double ig1;
            double dig1;
    };
    const Point points[3] = {
        {-1.5, 1.61959548076714e-12, 1.07972944614873e-11},
        {0.5, 6.93147180559945e-7, 3.33333333333333e-6},
        {1.0, 3.36838574941280e-6, 6.43703202889193e-6},
    };
    for (const Point& point : points)
    {
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current({point.vg1k, 400.0, 400.0}, currents);
        pentode.jacobian({point.vg1k, 400.0, 400.0}, jacobian);
        EXPECT_NEAR(currents[0], point.ig1, 1e-18 + 1e-9 * point.ig1) << point.vg1k;
        EXPECT_NEAR(jacobian[0][0], point.dig1, 1e-18 + 1e-9 * point.dig1) << point.vg1k;
    }
}

TEST(Unit_Dsp, PentodeEL34JacobianMatchesCentredDifferencesEverywhere)
{
    const SushiDSP::PentodeEL34 pentode;
    for (const double vg1k : {-45.0, -30.0, -15.0, -5.0, 0.0})
    {
        for (const double vg2k : {50.0, 200.0, 400.0})
        {
            for (const double vpk : {20.0, 100.0, 250.0, 450.0})
            {
                // The screen law kinks at z = 0; skip it. See tests/functional/unit/README.md.
                if (std::abs(vg2k / SushiDSP::PentodeEL34::K_MU + vg1k) < 1e-3)
                {
                    continue;
                }

                std::array<std::array<double, 3>, 3> jacobian{};
                pentode.jacobian({vg1k, vg2k, vpk}, jacobian);
                for (std::size_t input = 0; input < 3; ++input)
                {
                    const double step = 1e-6;
                    std::array<double, 3> up{vg1k, vg2k, vpk};
                    std::array<double, 3> down{vg1k, vg2k, vpk};
                    up[input] += step;
                    down[input] -= step;
                    std::array<double, 3> current_up{};
                    std::array<double, 3> current_down{};
                    pentode.current(up, current_up);
                    pentode.current(down, current_down);
                    for (std::size_t output = 0; output < 3; ++output)
                    {
                        const double numerical =
                            (current_up[output] - current_down[output]) / (2.0 * step);
                        EXPECT_NEAR(jacobian[output][input], numerical,
                                    1e-5 * std::abs(jacobian[output][input]) + 1e-12)
                            << vg1k << " " << vg2k << " " << vpk << " " << output << " " << input;
                    }
                }
            }
        }
    }
}

TEST(Unit_Dsp, PentodeEL34ScreenCurrentIsCOneAcrossItsKink)
{
    // kx > 1, so the screen current is C1 at z = 0. See tests/functional/unit/README.md.
    const SushiDSP::PentodeEL34 pentode;
    const double vg2k = 400.0;
    const double kink = -vg2k / SushiDSP::PentodeEL34::K_MU;
    for (const double offset : {-1e-2, -1e-4, -1e-6, 0.0, 1e-6, 1e-4, 1e-2})
    {
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current({kink + offset, vg2k, 400.0}, currents);
        pentode.jacobian({kink + offset, vg2k, 400.0}, jacobian);
        EXPECT_GE(currents[1], 0.0) << offset;
        EXPECT_LT(currents[1], 1e-6) << offset;
        EXPECT_GE(jacobian[1][0], 0.0) << offset;
        EXPECT_LT(jacobian[1][0], 1e-4) << offset;
        EXPECT_LT(std::abs(jacobian[1][1]), 1e-5) << offset;
    }

    std::array<double, 3> at_kink{};
    std::array<std::array<double, 3>, 3> jacobian_at_kink{};
    pentode.current({kink, vg2k, 400.0}, at_kink);
    pentode.jacobian({kink, vg2k, 400.0}, jacobian_at_kink);
    EXPECT_DOUBLE_EQ(at_kink[1], 0.0);
    EXPECT_DOUBLE_EQ(jacobian_at_kink[1][0], 0.0);
    EXPECT_DOUBLE_EQ(jacobian_at_kink[1][1], 0.0);
}

TEST(Unit_Dsp, PentodeEL34PlateCurrentKneesOverWithPlateVoltage)
{
    // Ip factors into a grid term times atan(Vpk/kvb). See tests/functional/unit/README.md.
    const SushiDSP::PentodeEL34 pentode;
    std::array<double, 3> high{};
    std::array<double, 3> low{};
    pentode.current({-20.0, 300.0, 400.0}, high);
    pentode.current({-20.0, 300.0, 100.0}, low);
    EXPECT_NEAR(high[2] / low[2], 1.13152342150114, 1e-9);
    EXPECT_DOUBLE_EQ(high[1], low[1]);
    EXPECT_DOUBLE_EQ(high[0], low[0]);
}

TEST(Unit_Dsp, PentodeEL34JointFormAgreesWithTheSeparateEntryPoints)
{
    // current() and jacobian() forward to the joint form. See tests/functional/unit/README.md.
    const SushiDSP::PentodeEL34 pentode;
    for (const double vg1k : {-45.0, -37.0, -20.0, -5.0, 0.0})
    {
        for (const double vg2k : {1e-4, 50.0, 300.0, 400.0})
        {
            for (const double vpk : {20.0, 100.0, 250.0, 400.0})
            {
                const std::array<double, 3> v{vg1k, vg2k, vpk};
                std::array<double, 3> separate_currents{};
                std::array<std::array<double, 3>, 3> separate_jacobian{};
                pentode.current(v, separate_currents);
                pentode.jacobian(v, separate_jacobian);
                std::array<double, 3> joint_currents{};
                std::array<std::array<double, 3>, 3> joint_jacobian{};
                pentode.current_and_jacobian(v, joint_currents, joint_jacobian);
                for (std::size_t branch = 0; branch < 3; ++branch)
                {
                    EXPECT_DOUBLE_EQ(joint_currents[branch], separate_currents[branch])
                        << vg1k << " " << vg2k << " " << vpk << " " << branch;
                    for (std::size_t input = 0; input < 3; ++input)
                    {
                        EXPECT_DOUBLE_EQ(joint_jacobian[branch][input],
                                         separate_jacobian[branch][input])
                            << vg1k << " " << vg2k << " " << vpk << " " << branch << " " << input;
                    }
                }
                EXPECT_DOUBLE_EQ(joint_jacobian[0][1], 0.0);
                EXPECT_DOUBLE_EQ(joint_jacobian[0][2], 0.0);
                EXPECT_DOUBLE_EQ(joint_jacobian[1][2], 0.0);
            }
        }
    }
    // The JCM800 idle point, straight out of the joint form (D6b Task 4's golden vector).
    std::array<double, 3> currents{};
    std::array<std::array<double, 3>, 3> jacobian{};
    pentode.current_and_jacobian({-37.0, 400.0, 400.0}, currents, jacobian);
    EXPECT_NEAR(currents[2], 3.34149261301e-2, 1e-11);
    EXPECT_NEAR(jacobian[2][0], 4.98316907313e-3, 1e-11);
    EXPECT_DOUBLE_EQ(currents[1], 0.0);
}

TEST(Unit_Dsp, PentodeEL34StaysFiniteUnderAbsurdInputs)
{
    const SushiDSP::PentodeEL34 pentode;
    for (const double vg1k : {-1e4, -50.0, 0.0, 50.0, 1e4})
    {
        for (const double vg2k : {-1e4, -1.0, 0.0, 1e-4, 1.0, 1e4})
        {
            for (const double vpk : {-1e4, 0.0, 1e4})
            {
                std::array<double, 3> currents{};
                std::array<std::array<double, 3>, 3> jacobian{};
                pentode.current({vg1k, vg2k, vpk}, currents);
                pentode.jacobian({vg1k, vg2k, vpk}, jacobian);
                for (std::size_t branch = 0; branch < 3; ++branch)
                {
                    EXPECT_TRUE(std::isfinite(currents[branch]))
                        << vg1k << " " << vg2k << " " << vpk << " " << branch;
                    for (std::size_t input = 0; input < 3; ++input)
                    {
                        EXPECT_TRUE(std::isfinite(jacobian[branch][input]))
                            << vg1k << " " << vg2k << " " << vpk << " " << branch << " " << input;
                    }
                }
            }
        }
    }
}
