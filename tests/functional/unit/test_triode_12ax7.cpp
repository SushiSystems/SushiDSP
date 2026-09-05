/**************************************************************************/
/* test_triode_12ax7.cpp                                                  */
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

#include "SushiDSP/circuits/models/Triode12AX7.hpp"

TEST(Unit_Dsp, Triode12AX7PlateCurrentMatchesKorenAtThreeOperatingPoints)
{
    // mu = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4.
    //
    // (1) Vpk = 250, Vgk = -1.5  (the classic 12AX7 quiescent point)
    //     root = sqrt(300 + 62500) = 250.599281722833
    //     t    = 600 * (0.01 - 1.5 / 250.599281722833) = 2.40860901989570
    //     s    = ln(1 + exp(t)) = 2.49473195572810,  E1 = 250 * s / 600 = 1.03947164822004
    //     Ip   = 2 * 1.03947164822004^1.4 / 1060 = 1.99187388684622e-3 A  (1.99 mA, right for a 12AX7)
    //     dIp/dVgk = 2.45547036171309e-3 S,  dIp/dVpk = 2.58169362128740e-5 S
    //     (gm = 2.46 mA/V and rp = 1/2.58e-5 = 38.7 kOhm, both a few percent off the datasheet's
    //      1.6 mA/V / 62.5 kOhm here - Koren's fit runs hot at this point, a known property of the
    //      model rather than a transcription error.)
    //
    // (2) Vpk = 100, Vgk = -2.0  (deep toward cutoff)
    //     root = sqrt(300 + 10000) = 101.488915650922,  t = 600 * (0.01 - 2 / root) = -5.82395133797152
    //     s = 2.95154218089098e-3,  E1 = 100 * s / 600 = 4.91923696815163e-4
    //     Ip = 2 * 4.91923696815163e-4^1.4 / 1060 = 4.40941496483551e-8 A
    //     dIp/dVgk = 3.64418892389800e-7 S,  dIp/dVpk = 7.69341309293717e-9 S
    //
    // (3) Vpk = 300, Vgk = 0  (grid at the cathode; the stage is fully on)
    //     root = sqrt(300 + 90000) = 300.499584026334,  t = 600 * 0.01 = 6 exactly
    //     s = ln(1 + exp(6)) = 6.00247568513773,  E1 = 300 * s / 600 = 3.00123784256887
    //     Ip = 2 * 3.00123784256887^1.4 / 1060 = 8.78910615164942e-3 A
    //     dIp/dVgk = 4.08295444882820e-3 S,  dIp/dVpk = 4.10158287076973e-5 S
    const SushiDSP::Triode12AX7 triode;
    struct Point
    {
        double vgk;
        double vpk;
        double ip;
        double dip_dvgk;
        double dip_dvpk;
    };
    const Point points[3] = {
        { -1.5, 250.0, 1.99187388684622e-3, 2.45547036171309e-3, 2.58169362128740e-5 },
        { -2.0, 100.0, 4.40941496483551e-8, 3.64418892389800e-7, 7.69341309293717e-9 },
        { 0.0, 300.0, 8.78910615164942e-3, 4.08295444882820e-3, 4.10158287076973e-5 },
    };
    for (const Point& point : points)
    {
        std::array<double, 2> currents{};
        std::array<std::array<double, 2>, 2> jacobian{};
        triode.current({ point.vgk, point.vpk }, currents);
        triode.jacobian({ point.vgk, point.vpk }, jacobian);
        EXPECT_NEAR(currents[1], point.ip, 1e-11 + 1e-9 * point.ip) << point.vpk;
        EXPECT_NEAR(jacobian[1][0], point.dip_dvgk, 1e-11 + 1e-9 * point.dip_dvgk) << point.vpk;
        EXPECT_NEAR(jacobian[1][1], point.dip_dvpk, 1e-13 + 1e-9 * point.dip_dvpk) << point.vpk;
        EXPECT_DOUBLE_EQ(jacobian[0][1], 0.0);
    }
}

TEST(Unit_Dsp, Triode12AX7GridCurrentFollowsNormansSoftLaw)
{
    // Ig = 1e-6 * ln(1 + exp((Vgk - 0.5)/0.15)), dIg/dVgk = (1e-6/0.15) * logistic(same argument).
    //
    //   Vgk = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
    //                Ig = 1.61959548076714e-12 A,  dIg = 1.07972944614873e-11 S (effectively cut off)
    //   Vgk =  0.5 : argument = 0, ln(2) = 0.693147180559945
    //                Ig = 6.93147180559945e-7 A,  dIg = (1e-6/0.15) * 0.5 = 3.33333333333333e-6 S
    //   Vgk =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
    //                Ig = 3.36838574941280e-6 A,  dIg = (1e-6/0.15) * 0.965554 = 6.43703202889193e-6 S
    const SushiDSP::Triode12AX7 triode;
    struct Point
    {
        double vgk;
        double ig;
        double dig;
    };
    const Point points[3] = {
        { -1.5, 1.61959548076714e-12, 1.07972944614873e-11 },
        { 0.5, 6.93147180559945e-7, 3.33333333333333e-6 },
        { 1.0, 3.36838574941280e-6, 6.43703202889193e-6 },
    };
    for (const Point& point : points)
    {
        std::array<double, 2> currents{};
        std::array<std::array<double, 2>, 2> jacobian{};
        triode.current({ point.vgk, 250.0 }, currents);
        triode.jacobian({ point.vgk, 250.0 }, jacobian);
        EXPECT_NEAR(currents[0], point.ig, 1e-18 + 1e-9 * point.ig) << point.vgk;
        EXPECT_NEAR(jacobian[0][0], point.dig, 1e-18 + 1e-9 * point.dig) << point.vgk;
    }
}

TEST(Unit_Dsp, Triode12AX7JacobianMatchesCentredDifferencesEverywhere)
{
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -8.0, -3.0, -1.0, 0.0, 1.0 })
    {
        for (const double vpk : { 5.0, 50.0, 150.0, 300.0, 400.0 })
        {
            std::array<std::array<double, 2>, 2> jacobian{};
            triode.jacobian({ vgk, vpk }, jacobian);
            for (std::size_t input = 0; input < 2; ++input)
            {
                const double step = 1e-6;
                std::array<double, 2> up{ vgk, vpk };
                std::array<double, 2> down{ vgk, vpk };
                up[input] += step;
                down[input] -= step;
                std::array<double, 2> current_up{};
                std::array<double, 2> current_down{};
                triode.current(up, current_up);
                triode.current(down, current_down);
                for (std::size_t output = 0; output < 2; ++output)
                {
                    const double numerical = (current_up[output] - current_down[output]) / (2.0 * step);
                    EXPECT_NEAR(jacobian[output][input], numerical,
                                1e-5 * std::abs(jacobian[output][input]) + 1e-12)
                        << vgk << " " << vpk << " " << output << " " << input;
                }
            }
        }
    }
}

TEST(Unit_Dsp, Triode12AX7IsCutOffAndCOneAtNonPositivePlateVoltage)
{
    const SushiDSP::Triode12AX7 triode;
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    triode.current({ -2.0, -10.0 }, currents);
    triode.jacobian({ -2.0, -10.0 }, jacobian);
    EXPECT_DOUBLE_EQ(currents[1], 0.0);
    EXPECT_DOUBLE_EQ(jacobian[1][0], 0.0);
    EXPECT_DOUBLE_EQ(jacobian[1][1], 0.0);
    // Approaching Vpk = 0 from above, plate current and both of its derivatives go to zero, so the
    // piecewise definition is C1 and not a slope discontinuity like the D5b clamp was.
    for (const double vpk : { 1e-3, 1e-4, 1e-5 })
    {
        triode.current({ -2.0, vpk }, currents);
        triode.jacobian({ -2.0, vpk }, jacobian);
        EXPECT_LT(currents[1], 1e-9);
        EXPECT_LT(std::abs(jacobian[1][0]), 1e-6);
    }
}

TEST(Unit_Dsp, Triode12AX7JointFormAgreesWithTheSeparateEntryPoints)
{
    // current() and jacobian() forward to the joint form, so these agree exactly by construction
    // today. Pinning that is the point: the three entry points cannot drift apart later, and a
    // current() specialised for speed would have to keep matching the form the solver calls.
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -8.0, -3.0, -1.5, 0.0, 1.0 })
    {
        for (const double vpk : { -10.0, 5.0, 100.0, 250.0, 400.0 })
        {
            const std::array<double, 2> v{ vgk, vpk };
            std::array<double, 2> separate_currents{};
            std::array<std::array<double, 2>, 2> separate_jacobian{};
            triode.current(v, separate_currents);
            triode.jacobian(v, separate_jacobian);
            std::array<double, 2> joint_currents{};
            std::array<std::array<double, 2>, 2> joint_jacobian{};
            triode.current_and_jacobian(v, joint_currents, joint_jacobian);
            for (std::size_t branch = 0; branch < 2; ++branch)
            {
                EXPECT_DOUBLE_EQ(joint_currents[branch], separate_currents[branch])
                    << vgk << " " << vpk << " " << branch;
                for (std::size_t input = 0; input < 2; ++input)
                {
                    EXPECT_DOUBLE_EQ(joint_jacobian[branch][input],
                                     separate_jacobian[branch][input])
                        << vgk << " " << vpk << " " << branch << " " << input;
                }
            }
            EXPECT_DOUBLE_EQ(joint_jacobian[0][1], 0.0);
        }
    }
    // The classic quiescent point, straight out of the joint form (D6b Task 3's golden vector).
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    triode.current_and_jacobian({ -1.5, 250.0 }, currents, jacobian);
    EXPECT_NEAR(currents[1], 1.99187388685e-3, 1e-11);
    EXPECT_NEAR(jacobian[1][0], 2.45547036171e-3, 1e-11);
    EXPECT_NEAR(jacobian[1][1], 2.58169362129e-5, 1e-13);
}

TEST(Unit_Dsp, Triode12AX7StaysFiniteUnderAbsurdInputs)
{
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -1e4, -50.0, 50.0, 1e4 })
    {
        for (const double vpk : { -1e4, 0.0, 1e4 })
        {
            std::array<double, 2> currents{};
            std::array<std::array<double, 2>, 2> jacobian{};
            triode.current({ vgk, vpk }, currents);
            triode.jacobian({ vgk, vpk }, jacobian);
            EXPECT_TRUE(std::isfinite(currents[0]) && std::isfinite(currents[1])) << vgk << " " << vpk;
            EXPECT_TRUE(std::isfinite(jacobian[0][0]) && std::isfinite(jacobian[1][1])) << vgk << " " << vpk;
        }
    }
}
