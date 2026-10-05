/****************************************************************/
/* test_dk_op_amp.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_dk_op_amp.cpp
 * @brief Tests for the ideal op-amp nullor element in DkNetlist.
 * @author Mustafa Garip & Sushi Systems
 */

#include <array>
#include <cmath>
#include <cstddef>

#include <gtest/gtest.h>

#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

namespace
{
    /// The empty nonlinear port every purely linear DK block is stepped with. It spells out the
    /// glue contract at N = 0 to match the ports in the other DK tests; DkCircuit never calls
    /// either method at that width.
    struct NoNonlinearity
    {
        /// @brief Evaluates no branch currents.
        void currents(const std::array<double, 0>&, std::array<double, 0>&) const {}

        /// @brief Evaluates no Jacobian entries.
        void jacobian(const std::array<double, 0>&, std::array<std::array<double, 0>, 0>&) const {}
    };

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double kPi = 3.14159265358979323846;
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpInvertingAmplifierHasGainMinusRfOverRin)
{
    // Node 1 = source, 2 = the summing junction, 3 = the output. The nullator senses ground
    // against node 2, so V(2) = 0; the norator drives node 3. KCL at node 2 is then
    // -V(1)/Rin - V(3)/Rf = 0, so V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, 10.0e3);
    netlist.resistor(2, 3, 100.0e3);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity        nonlinear;
    std::array<double, 1> outputs{};
    for (const double input : { -3.0, -0.25, 0.0, 0.25, 3.0 })
    {
        circuit.process_sample(nonlinear, { input }, outputs);
        EXPECT_NEAR(outputs[0], -10.0 * input, 1e-12) << input;
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpNonInvertingAmplifierHasGainOnePlusRfOverRg)
{
    // The Tube Screamer clip stage with its capacitors removed and the drive pot at zero:
    // 1 + 51000/4700 = 11.851063829787234 exactly.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(3, 2, 51.0e3);
    netlist.resistor(2, 0, 4700.0);
    netlist.nullor(1, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity        nonlinear;
    std::array<double, 1> outputs{};
    circuit.process_sample(nonlinear, { 1.0 }, outputs);
    EXPECT_NEAR(outputs[0], 11.851063829787234, 1e-11);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpVoltageFollowerPassesItsInputThrough)
{
    // The degenerate case: the sensed node and the driven node are the same one.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(2, 0, 10.0e3);
    netlist.nullor(1, 2, 2, 0);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 48000.0));

    NoNonlinearity        nonlinear;
    std::array<double, 1> outputs{};
    circuit.process_sample(nonlinear, { 0.7 }, outputs);
    EXPECT_NEAR(outputs[0], 0.7, 1e-12);
}

TEST(Unit_Dsp, DkCircuitIdealOpAmpLowPassMatchesTheBilinearTransformExactly)
{
    // Inverting stage with Rf in parallel with Cf: H(s) = -(Rf/Rin) / (1 + s Rf Cf).
    // Trapezoidal DK must reproduce its bilinear transform exactly. With Rin = 10 kOhm,
    // Rf = 100 kOhm, Cf = 10 nF, fs = 48000: a = 2 fs Rf Cf = 2*48000*1e-3 = 96, so
    //   H(z) = -(Rf/Rin) (1 + z^-1) / ((1 + a) + (1 - a) z^-1)
    // with pole (a - 1)/(a + 1) = 95/97 = 0.979381443298969 and DC gain -10 exactly.
    // The analogue corner is 1/(2 pi Rf Cf) = 159.154943 Hz.
    constexpr double kInputOhms = 10.0e3;
    constexpr double kFeedbackOhms = 100.0e3;
    constexpr double kFeedbackFarads = 10.0e-9;
    constexpr double kSampleRate = 48000.0;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kInputOhms);
    netlist.resistor(2, 3, kFeedbackOhms);
    netlist.capacitor(2, 3, kFeedbackFarads);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    const double alpha = 2.0 * kSampleRate * kFeedbackOhms * kFeedbackFarads;
    const double gain = kFeedbackOhms / kInputOhms;
    double       reference_input = 0.0;
    double       reference_output = 0.0;

    NoNonlinearity nonlinear;
    for (int sample = 0; sample < 4096; ++sample)
    {
        const double input = std::sin(2.0 * kPi * 137.0 * sample / kSampleRate)
                             + 0.3 * std::sin(2.0 * kPi * 6000.0 * sample / kSampleRate);
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, { input }, outputs);
        reference_output = (-gain * (input + reference_input) - (1.0 - alpha) * reference_output)
                           / (1.0 + alpha);
        reference_input = input;
        ASSERT_NEAR(outputs[0], reference_output, 1e-11) << sample;
    }
}

TEST(Unit_Dsp, DkCircuitRejectsAnOpAmpWithNoFeedbackPath)
{
    // An op-amp whose output does not reach its own input leaves the augmented system rank
    // deficient. prepare() must say so rather than derive nonsense.
    SushiDSP::dk::DkNetlist<0, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, 10.0e3);
    netlist.resistor(3, 0, 10.0e3);
    netlist.nullor(0, 2, 3, 0);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<0, 0, 1, 1> circuit;
    EXPECT_FALSE(circuit.prepare(netlist, 48000.0));
}
