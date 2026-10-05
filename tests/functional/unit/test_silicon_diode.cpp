/****************************************************************/
/* test_silicon_diode.cpp                                       */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"

TEST(Unit_Dsp, SiliconDiodeMatchesShockleyAtThreeOperatingPoints)
{
    // 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V, so n*Vt = 0.045313728 V exactly.
    //
    //  v = 0.4 V : v/(nVt) = 8.82734...  exp = 6.81817404536e3
    //              i  = 2.52e-9 * (6818.17404536 - 1)      = 1.71792785943e-5 A
    //              di = (2.52e-9 / 0.045313728) * 6818.174 = 3.79174244818e-4 S
    //  v = 0.6 V : v/(nVt) = 13.2410...  exp = 5.6299187171e5
    //              i  = 2.52e-9 * (562991.87171 - 1)       = 1.41873699671e-3 A
    //              di = (2.52e-9 / 0.045313728) * 562991.9 = 3.13092649696e-2 S
    //  v = 0.7 V : v/(nVt) = 15.4479...  exp = 5.11586582342e6
    //              i  = 2.52e-9 * (5115865.82342 - 1)      = 1.2891979355e-2 A
    //              di = (2.52e-9 / 0.045313728) * 5115866  = 2.84504993167e-1 S
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
    // v = -0.5 V: exp(-11.0342) = 1.61404223972e-5, i = 2.52e-9 * (1.614e-5 - 1)
    //           = -2.51995932614e-9 A, di = (2.52e-9/0.045313728)*1.614e-5 = 8.97605786067e-13 S
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
    // The extrapolation knee sits at v_max = 40 * n * Vt = 1.81254912 V; the derivative must be
    // continuous across it. knee_jacobian_ itself is ~1.309e10 S (Is/(nVt) * exp(40)), so a fixed
    // absolute tolerance of 1e-3/1e-9 is unsatisfiable at this magnitude for any correct
    // implementation: measured directly, jacobian(knee - 1e-7) and jacobian(knee + 1e-7) differ by
    // ~2.89e4 absolute (~2.2e-6 relative) purely from the Shockley curve's real slope this close to
    // exp(40) — a 1e-4 relative tolerance (~1.31e6 absolute) left over 45x slack versus that
    // measured gap, so this uses 1e-5 relative (~1.31e5 absolute, ~4.5x the measured gap) instead.
    // The extrapolation identity below only holds to double's rounding: measured directly, the two
    // sides differ by ~4e-16 relative (~1 ULP at this magnitude), so this uses 1e-12 relative —
    // ~2500x the measured gap, still ~1e3x tighter than the 1e-9 absolute tolerance originally in
    // the brief once rescaled to this ~7e9 magnitude.
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
    // The D6c clip stage's nonlinear port is v[0] = w, v[1] = -w across one pair, so the pair's
    // total current is Is(e^(w/nVt) - 1) - Is(e^(-w/nVt) - 1) = 2 Is sinh(w / nVt) and its
    // conductance is jacobian(w) + jacobian(-w). n*Vt = 1.752 * 0.025864 = 0.045313728 V, so
    // 1/(n*Vt) = 22.068367449. Values computed at double precision in the D6c plan:
    //   w = 0.2  : i = 2.080515143e-7 A,  di/dw = 4.592704262e-6 S
    //   w = 0.3  : i = 1.890822805e-6 A,  di/dw = 4.172752069e-5 S
    //   w = 0.38 : i = 1.105058316e-5 A,  di/dw = 2.438683551e-4 S
    //   w = 0.4  : i = 1.718179822e-5 A,  di/dw = 3.791742530e-4 S
    //   w = 0.5  : i = 1.561297429e-4 A,  di/dw = 3.445528538e-3 S
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
    // n = 1 makes n*Vt = 0.025864; at v = 0.6 the exponent is 23.1983..., exp = 1.19426e10,
    // so the current is far larger than the 1N914's at the same voltage.
    const SushiDSP::SiliconDiode ideal(2.52e-9, 1.0, 0.025864);
    const SushiDSP::SiliconDiode part;
    EXPECT_GT(ideal.current(0.6), 100.0 * part.current(0.6));
}

TEST(Unit_Dsp, SiliconDiodeAntiderivativeDifferentiatesBackToTheDiodeCurrent)
{
    // The whole point of the antiderivative is that its slope is the current, so a centred
    // difference of it has to return the current the model itself reports. The sweep crosses both
    // knees (+-1.8125 V for the defaults), so it covers the Shockley branch and both quadratic
    // extrapolations rather than only the range a Tube Screamer visits.
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
    // A step in the antiderivative would put a spike straight into the divided difference
    // antiderivative antialiasing takes, so neither knee may leave one. The bound is the largest
    // slope on the interval times its width, which is what differentiability across the knee
    // means.
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
    // current_and_antiderivative() exists to share one exponential; it must not drift from the
    // two entry points that forward to it.
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
    // Below the lower knee the model continues the reverse-biased diode it actually is: a current
    // that stays at -Is and a conductance of (Is/(n*Vt))*exp(-40), some 34 orders of magnitude
    // under the forward tangent's. The two tangents are not each other's mirror image, and the
    // earlier code that made them one put a step from -2.5 nA to -593 MA right here (D6c C2).
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
