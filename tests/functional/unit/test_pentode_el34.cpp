/**************************************************************************/
/* test_pentode_el34.cpp                                                  */
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


#include <array>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/models/PentodeEL34.hpp"

TEST(Unit_Dsp, PentodeEL34MatchesKorenAtThreeOperatingPoints)
{
    // mu = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35.
    //
    // (1) Vpk = 400, Vg2k = 400, Vg1k = -37  (a JCM800 idle point at -37 V fixed bias)
    //     u_arg = 60*(1/11 - 37/400) = -0.0954545454545452
    //     s = ln(1+exp(u_arg)) = 0.646558421976595,  E1 = 400*s/60 = 4.31038947984397
    //     knee = atan(400/24) = 1.51086817167369
    //     Ip  = 2 * E1^1.35 / 650 * knee = 3.34149261300818e-2 A   (33.4 mA idle)
    //     dIp/dVg1k = 4.98316907313410e-3 S,  dIp/dVg2k = 5.73718514953930e-4 S,
    //     dIp/dVpk  = 3.30555615548759e-6 S   (a ~300 kOhm plate resistance: pentode, as expected)
    //     z = 400/11 - 37 = -0.636363636363633 < 0, so Ig2 = 0 and its whole row is 0 - the screen
    //     draws nothing at idle, which is what the max(0, .) clamp is for.
    //
    // (2) Vpk = 250, Vg2k = 300, Vg1k = -20
    //     u_arg = 60*(1/11 - 20/300) = 1.45454545454545,  s = 1.66440636412608
    //     E1 = 300*s/60 = 8.32203182063039,  knee = atan(250/24) = 1.47508961870809
    //     Ip = 7.92947981392609e-2 A,  dIp/dVg1k = 1.04281593600714e-2 S,
    //     dIp/dVg2k = 1.05203721563143e-3 S, dIp/dVpk = 2.04537709631731e-5 S
    //     z = 300/11 - 20 = 7.27272727272727,  Ig2 = z^1.35 / 4500 = 3.23652408855726e-3 A
    //     dIg2/dVg1k = 1.35 * z^0.35 / 4500 = 6.00779783938442e-4 S
    //     dIg2/dVg2k = 6.00779783938442e-4 / 11 = 5.46163439944038e-5 S
    //
    // (3) Vpk = 100, Vg2k = 400, Vg1k = -5   (hard into conduction, plate pulled down)
    //     u_arg = 60*(1/11 - 5/400) = 4.70454545454546,  s = 4.71355874131221
    //     E1 = 400*s/60 = 31.4237249420814,  knee = atan(100/24) = 1.33525134607403
    //     Ip = 4.31496731894557e-1 A,  dIp/dVg1k = 1.83712705886168e-2 S,
    //     dIp/dVg2k = 1.68594235250184e-3 S,  dIp/dVpk = 7.33338162266083e-4 S
    //     z = 400/11 - 5 = 31.3636363636364,  Ig2 = 2.32789367120296e-2 A,
    //     dIg2/dVg1k = 1.00200640630040e-3 S,  dIg2/dVg2k = 9.10914914818548e-5 S
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
        { -37.0, 400.0, 400.0, 3.34149261300818e-2, 4.98316907313410e-3, 5.73718514953930e-4,
          3.30555615548759e-6, 0.0, 0.0, 0.0 },
        { -20.0, 300.0, 250.0, 7.92947981392609e-2, 1.04281593600714e-2, 1.05203721563143e-3,
          2.04537709631731e-5, 3.23652408855726e-3, 6.00779783938442e-4, 5.46163439944038e-5 },
        { -5.0, 400.0, 100.0, 4.31496731894557e-1, 1.83712705886168e-2, 1.68594235250184e-3,
          7.33338162266083e-4, 2.32789367120296e-2, 1.00200640630040e-3, 9.10914914818548e-5 },
    };
    for (const Point& point : points)
    {
        const std::array<double, 3> voltages{ point.vg1k, point.vg2k, point.vpk };
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
    // Ig1 = 1e-6 * ln(1 + exp((Vg1k - 0.5)/0.15)), dIg1/dVg1k = (1e-6/0.15) * logistic(same).
    //
    //   Vg1k = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
    //                 Ig1 = 1.61959548076714e-12 A,  dIg1 = 1.07972944614873e-11 S
    //   Vg1k =  0.5 : argument = 0, ln(2) = 0.693147180559945
    //                 Ig1 = 6.93147180559945e-7 A,  dIg1 = (1e-6/0.15)*0.5 = 3.33333333333333e-6 S
    //   Vg1k =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
    //                 Ig1 = 3.36838574941280e-6 A,  dIg1 = 6.43703202889193e-6 S
    const SushiDSP::PentodeEL34 pentode;
    struct Point
    {
        double vg1k;
        double ig1;
        double dig1;
    };
    const Point points[3] = {
        { -1.5, 1.61959548076714e-12, 1.07972944614873e-11 },
        { 0.5, 6.93147180559945e-7, 3.33333333333333e-6 },
        { 1.0, 3.36838574941280e-6, 6.43703202889193e-6 },
    };
    for (const Point& point : points)
    {
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current({ point.vg1k, 400.0, 400.0 }, currents);
        pentode.jacobian({ point.vg1k, 400.0, 400.0 }, jacobian);
        EXPECT_NEAR(currents[0], point.ig1, 1e-18 + 1e-9 * point.ig1) << point.vg1k;
        EXPECT_NEAR(jacobian[0][0], point.dig1, 1e-18 + 1e-9 * point.dig1) << point.vg1k;
    }
}

TEST(Unit_Dsp, PentodeEL34JacobianMatchesCentredDifferencesEverywhere)
{
    const SushiDSP::PentodeEL34 pentode;
    for (const double vg1k : { -45.0, -30.0, -15.0, -5.0, 0.0 })
    {
        for (const double vg2k : { 50.0, 200.0, 400.0 })
        {
            for (const double vpk : { 20.0, 100.0, 250.0, 450.0 })
            {
                // The screen law has a max(0, z) kink at z = Vg2k/mu + Vg1k = 0. A centred
                // difference that straddles the kink averages two different one-sided slopes and
                // is not the derivative there, so skip a step-sized neighbourhood of it.
                if (std::abs(vg2k / SushiDSP::PentodeEL34::kMu + vg1k) < 1e-3)
                {
                    continue;
                }

                std::array<std::array<double, 3>, 3> jacobian{};
                pentode.jacobian({ vg1k, vg2k, vpk }, jacobian);
                for (std::size_t input = 0; input < 3; ++input)
                {
                    const double step = 1e-6;
                    std::array<double, 3> up{ vg1k, vg2k, vpk };
                    std::array<double, 3> down{ vg1k, vg2k, vpk };
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
    // z = Vg2k/mu + Vg1k crosses zero at Vg1k = -Vg2k/mu. kx = 1.35 > 1, so z^kx and
    // kx*z^(kx-1) both vanish continuously there: no slope step for the Newton solver to trip on.
    const SushiDSP::PentodeEL34 pentode;
    const double vg2k = 400.0;
    const double kink = -vg2k / SushiDSP::PentodeEL34::kMu;
    for (const double offset : { -1e-2, -1e-4, -1e-6, 0.0, 1e-6, 1e-4, 1e-2 })
    {
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current({ kink + offset, vg2k, 400.0 }, currents);
        pentode.jacobian({ kink + offset, vg2k, 400.0 }, jacobian);
        EXPECT_GE(currents[1], 0.0) << offset;
        EXPECT_LT(currents[1], 1e-6) << offset;
        EXPECT_GE(jacobian[1][0], 0.0) << offset;
        EXPECT_LT(jacobian[1][0], 1e-4) << offset;
        EXPECT_LT(std::abs(jacobian[1][1]), 1e-5) << offset;
    }

    std::array<double, 3> at_kink{};
    std::array<std::array<double, 3>, 3> jacobian_at_kink{};
    pentode.current({ kink, vg2k, 400.0 }, at_kink);
    pentode.jacobian({ kink, vg2k, 400.0 }, jacobian_at_kink);
    EXPECT_DOUBLE_EQ(at_kink[1], 0.0);
    EXPECT_DOUBLE_EQ(jacobian_at_kink[1][0], 0.0);
    EXPECT_DOUBLE_EQ(jacobian_at_kink[1][1], 0.0);
}

TEST(Unit_Dsp, PentodeEL34PlateCurrentKneesOverWithPlateVoltage)
{
    // The pentode's whole character is atan(Vpk/kvb): Ip factors into a grid-only term times that
    // knee, so quadrupling the plate voltage raises Ip only by
    // atan(400/24)/atan(100/24) = 1.51086817167369 / 1.33525134607403 = 1.13152342150114.
    // A triode, whose plate current tracks Vpk directly, would have risen far more.
    const SushiDSP::PentodeEL34 pentode;
    std::array<double, 3> high{};
    std::array<double, 3> low{};
    pentode.current({ -20.0, 300.0, 400.0 }, high);
    pentode.current({ -20.0, 300.0, 100.0 }, low);
    EXPECT_NEAR(high[2] / low[2], 1.13152342150114, 1e-9);
    EXPECT_DOUBLE_EQ(high[1], low[1]);
    EXPECT_DOUBLE_EQ(high[0], low[0]);
}

TEST(Unit_Dsp, PentodeEL34StaysFiniteUnderAbsurdInputs)
{
    const SushiDSP::PentodeEL34 pentode;
    for (const double vg1k : { -1e4, -50.0, 0.0, 50.0, 1e4 })
    {
        for (const double vg2k : { -1e4, -1.0, 0.0, 1e-4, 1.0, 1e4 })
        {
            for (const double vpk : { -1e4, 0.0, 1e4 })
            {
                std::array<double, 3> currents{};
                std::array<std::array<double, 3>, 3> jacobian{};
                pentode.current({ vg1k, vg2k, vpk }, currents);
                pentode.jacobian({ vg1k, vg2k, vpk }, jacobian);
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
