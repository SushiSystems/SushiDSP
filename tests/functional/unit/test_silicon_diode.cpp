/**************************************************************************/
/* test_silicon_diode.cpp                                                 */
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
 * @file test_silicon_diode.cpp
 * @brief Tests the SiliconDiode model against hand-computed currents and derivatives.
 * @author Mustafa Garip
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"

TEST(Unit_Dsp, SiliconDiodeMatchesShockleyAtThreeOperatingPoints)
{
    // 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    EXPECT_NEAR(diode.current(0.4), 1.71792785943e-5, 1e-16);
    EXPECT_NEAR(diode.jacobian(0.4), 3.79174244818e-4, 1e-15);
    EXPECT_NEAR(diode.current(0.6), 1.41873699671e-3, 1e-14);
    EXPECT_NEAR(diode.jacobian(0.6), 3.13092649696e-2, 1e-13);
    EXPECT_NEAR(diode.current(0.7), 1.2891979355e-2, 1e-13);
    EXPECT_NEAR(diode.jacobian(0.7), 2.84504993167e-1, 1e-12);
}

TEST(Unit_Dsp, SiliconDiodeReverseCurrentApproachesMinusSaturationCurrent)
{
    // At v = -0.5 V the current is -2.51995932614e-9 A. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    EXPECT_NEAR(diode.current(-0.5), -2.51995932614e-9, 1e-20);
    EXPECT_NEAR(diode.jacobian(-0.5), 8.97605786067e-13, 1e-24);
    EXPECT_NEAR(diode.current(0.0), 0.0, 1e-24);
}

TEST(Unit_Dsp, SiliconDiodeJacobianMatchesACentredDifference)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -0.3, 0.0, 0.3, 0.55, 0.65 })
    {
        const double step = 1e-6;
        const double numerical = (diode.current(voltage + step) - diode.current(voltage - step)) / (2.0 * step);
        EXPECT_NEAR(diode.jacobian(voltage), numerical, 1e-6 * std::abs(diode.jacobian(voltage)) + 1e-14) << voltage;
    }
}

TEST(Unit_Dsp, SiliconDiodeStaysFiniteAndCOneAcrossTheLinearExtrapolation)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -60.0, -2.0, 2.0, 60.0, 1000.0 })
    {
        EXPECT_TRUE(std::isfinite(diode.current(voltage))) << voltage;
        EXPECT_TRUE(std::isfinite(diode.jacobian(voltage))) << voltage;
    }
    // The knee sits at v_max = 40 * n * Vt = 1.81254912 V. See tests/functional/unit/README.md.
    const double knee = 40.0 * 1.752 * 0.025864;
    const double jacobian_at_knee = diode.jacobian(knee);
    EXPECT_NEAR(diode.jacobian(knee - 1e-7), diode.jacobian(knee + 1e-7), 1e-5 * jacobian_at_knee);
    EXPECT_NEAR(diode.current(knee + 0.5), diode.current(knee) + 0.5 * diode.jacobian(knee),
                1e-12 * diode.current(knee + 0.5));
}

TEST(Unit_Dsp, SiliconDiodeJointFormAgreesWithTheSeparateEntryPoints)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -60.0, -0.5, 0.0, 0.4, 0.6, 0.7, 1.81254912, 60.0 })
    {
        double current = 0.0;
        double conductance = 0.0;
        diode.current_and_jacobian(voltage, current, conductance);
        EXPECT_DOUBLE_EQ(current, diode.current(voltage)) << voltage;
        EXPECT_DOUBLE_EQ(conductance, diode.jacobian(voltage)) << voltage;
    }
    // The same three Shockley points the D6b case pins, read out of the joint form.
    double current = 0.0;
    double conductance = 0.0;
    diode.current_and_jacobian(0.4, current, conductance);
    EXPECT_NEAR(current, 1.71792785943e-5, 1e-16);
    EXPECT_NEAR(conductance, 3.79174244818e-4, 1e-15);
    diode.current_and_jacobian(0.6, current, conductance);
    EXPECT_NEAR(current, 1.41873699671e-3, 1e-14);
    EXPECT_NEAR(conductance, 3.13092649696e-2, 1e-13);
    diode.current_and_jacobian(0.7, current, conductance);
    EXPECT_NEAR(current, 1.2891979355e-2, 1e-13);
    EXPECT_NEAR(conductance, 2.84504993167e-1, 1e-12);
}

TEST(Unit_Dsp, SiliconDiodeAntiparallelPairMatchesTwoTimesSaturationCurrentTimesSinh)
{
    // The pair's total current is 2 Is sinh(w / nVt). See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    struct Point
    {
        double w;
        double current;
        double conductance;
    };
    const Point points[5] = {
        { 0.2, 2.080515143e-7, 4.592704262e-6 },
        { 0.3, 1.890822805e-6, 4.172752069e-5 },
        { 0.38, 1.105058316e-5, 2.438683551e-4 },
        { 0.4, 1.718179822e-5, 3.791742530e-4 },
        { 0.5, 1.561297429e-4, 3.445528538e-3 },
    };
    for (const Point& point : points)
    {
        double forward_current = 0.0;
        double forward_conductance = 0.0;
        double reverse_current = 0.0;
        double reverse_conductance = 0.0;
        diode.current_and_jacobian(point.w, forward_current, forward_conductance);
        diode.current_and_jacobian(-point.w, reverse_current, reverse_conductance);
        EXPECT_NEAR(forward_current - reverse_current, point.current, 1e-9 * point.current)
            << point.w;
        EXPECT_NEAR(forward_conductance + reverse_conductance, point.conductance,
                    1e-9 * point.conductance)
            << point.w;
    }
}

TEST(Unit_Dsp, SiliconDiodeHonoursANonDefaultIdealityFactor)
{
    // n = 1 makes n*Vt = 0.025864 V. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode ideal(2.52e-9, 1.0, 0.025864);
    const SushiDSP::SiliconDiode part;
    EXPECT_GT(ideal.current(0.6), 100.0 * part.current(0.6));
}

TEST(Unit_Dsp, SiliconDiodeAntiderivativeDifferentiatesBackToTheDiodeCurrent)
{
    // The antiderivative's slope is the diode current. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    const double                 delta = 1.0e-7;
    for (double voltage = -2.2; voltage <= 2.2; voltage += 0.01)
    {
        const double slope = (diode.current_antiderivative(voltage + delta)
                              - diode.current_antiderivative(voltage - delta))
                             / (2.0 * delta);
        const double expected = diode.current(voltage);
        EXPECT_NEAR(slope, expected, 1.0e-6 * std::fabs(expected) + 1.0e-14) << voltage;
    }
}

TEST(Unit_Dsp, SiliconDiodeAntiderivativeHasNoStepAtEitherKnee)
{
    // The bound is the largest slope times the width. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    const double                 knee = 40.0 * 1.752 * 0.025864;
    const double                 delta = 1.0e-9;
    for (const double edge : { knee, -knee })
    {
        const double below = diode.current_antiderivative(edge - delta);
        const double above = diode.current_antiderivative(edge + delta);
        const double steepest
            = std::max(std::fabs(diode.current(edge - delta)), std::fabs(diode.current(edge + delta)));
        EXPECT_LE(std::fabs(above - below), 2.0 * delta * steepest * 1.000001) << edge;
    }
}

TEST(Unit_Dsp, SiliconDiodeJointAntiderivativeFormAgreesWithTheSeparateEntryPoints)
{
    // The joint form must match the two entry points. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -2.5, -0.7, -0.05, 0.0, 0.05, 0.7, 2.5 })
    {
        double current = 0.0;
        double antiderivative = 0.0;
        diode.current_and_antiderivative(voltage, current, antiderivative);
        EXPECT_DOUBLE_EQ(current, diode.current(voltage)) << voltage;
        EXPECT_DOUBLE_EQ(antiderivative, diode.current_antiderivative(voltage)) << voltage;
    }
}

TEST(Unit_Dsp, SiliconDiodeReverseExtrapolationContinuesTheShockleyLawRatherThanMirroringForward)
{
    // Below the lower knee the current stays at -Is. See tests/functional/unit/README.md.
    const SushiDSP::SiliconDiode diode;
    const double                 knee = 40.0 * 1.752 * 0.025864;
    const double                 saturation = 2.52e-9;

    // C^0 and C^1 across the lower knee, to the same relative tolerance the upper knee is held to.
    const double inside_current = diode.current(-knee + 1e-7);
    const double outside_current = diode.current(-knee - 1e-7);
    EXPECT_NEAR(outside_current, inside_current, 1e-5 * std::fabs(inside_current));
    EXPECT_NEAR(diode.jacobian(-knee - 1e-7), diode.jacobian(-knee + 1e-7),
                1e-5 * diode.jacobian(-knee + 1e-7));

    // Far into reverse bias the current is still -Is, not a runaway.
    for (const double voltage : { -2.0, -10.0, -1000.0 })
    {
        EXPECT_NEAR(diode.current(voltage), -saturation, 1e-6 * saturation) << voltage;
        EXPECT_LT(diode.jacobian(voltage), 1e-20) << voltage;
    }
}
