/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests DkCircuit on linear, resonant and diode circuits with closed-form answers.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>

#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

namespace
{
    /// A deliberately trivial nonlinear port: a linear conductance. It exercises the glue
    /// contract and the Newton path without depending on any circuit model, which keeps this
    /// instantiation gate independent of Tasks 2 to 4.
    template <std::size_t N> struct LinearPort
    {
            double conductance = 1e-6;

            void currents_and_jacobian(const std::array<double, N>& v, std::array<double, N>& i,
                                       std::array<std::array<double, N>, N>& j) const
            {
                for (std::size_t index = 0; index < N; ++index)
                {
                    i[index] = conductance * v[index];
                    j[index][index] = conductance;
                }
            }
    };

    /// How far the DK diode-clipper solution may sit from a true root of the scalar oracle's
    /// own equation, in volts: |residual| divided by the residual's slope is a first-order
    /// bound on the distance to the root, which is the quantity that actually matters and is
    /// comparable between the two solvers. Measured worst case over the test's sweep is
    /// 9.7e-13 V, so this leaves an order of magnitude of margin.
    constexpr double K_DIODE_SOLUTION_ERROR_VOLTS = 1e-11;

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// Two antiparallel SiliconDiode branches sharing one device model. Branch 0 sees
    /// v = V(a) - V(b) and branch 1 sees the negated node pair, so the pair reproduces the
    /// symmetric law i(v) = Is*(exp(v/Vt) - exp(-v/Vt)) that DiodeClipperCircuit solves: the
    /// two Shockley -1 offsets cancel because the branches conduct in opposite directions.
    /// The Jacobian is diagonal because the two devices are independent.
    struct AntiparallelDiodePort
    {
            SushiDSP::SiliconDiode diode;

            /// @param model The device model both branches share.
            explicit AntiparallelDiodePort(const SushiDSP::SiliconDiode& model) : diode(model) {}

            /// @brief Evaluates both branch currents and the diagonal branch Jacobian.
            /// @param v Branch voltages.
            /// @param i Branch currents, overwritten.
            /// @param j Jacobian, its off-diagonal entries already zeroed by DkCircuit.
            void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                       std::array<std::array<double, 2>, 2>& j) const
            {
                diode.current_and_jacobian(v[0], i[0], j[0][0]);
                diode.current_and_jacobian(v[1], i[1], j[1][1]);
            }
    };

    /// A single-branch linear port that can be told to return a non-finite current for one
    /// sample, so the NaN fallback path can be exercised without an unstable circuit.
    struct PoisonablePort
    {
            double conductance = 1e-4; ///< Branch conductance while healthy.
            bool poisoned = false;     ///< When set, the branch current is infinity.

            /// @brief Evaluates the branch current, or infinity while poisoned, and the 1x1 Jacobian.
            /// @param v Branch voltages.
            /// @param i Branch currents, overwritten.
            /// @param j Jacobian, overwritten.
            void currents_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& i,
                                       std::array<std::array<double, 1>, 1>& j) const
            {
                i[0] = poisoned ? std::numeric_limits<double>::infinity() : conductance * v[0];
                j[0][0] = conductance;
            }
    };

    /// @brief Returns the magnitude of the DTFT of a real impulse response at one frequency.
    /// @param impulse The impulse response, assumed fully decayed by its last sample.
    /// @param frequency_hz Frequency to evaluate at.
    /// @param sample_rate Sample rate the response was measured at.
    /// @return The magnitude, a pure number.
    /// @see tests/foundation/math/dk/test_dk_circuit/README.md
    double magnitude_at(const std::vector<double>& impulse, double frequency_hz, double sample_rate)
    {
        const double omega = 2.0 * K_PI * frequency_hz / sample_rate;
        double real = 0.0;
        double imaginary = 0.0;
        for (std::size_t n = 0; n < impulse.size(); ++n)
        {
            const double phase = omega * static_cast<double>(n);
            real += impulse[n] * std::cos(phase);
            imaginary -= impulse[n] * std::sin(phase);
        }
        return std::sqrt(real * real + imaginary * imaginary);
    }

    /// @brief Locates the frequency of the largest magnitude by a two-stage grid scan.
    /// @param impulse The impulse response.
    /// @param sample_rate Sample rate the response was measured at.
    /// @param low Lower bound of the coarse search, scanned in 2 Hz steps.
    /// @param high Upper bound of the coarse search.
    /// @return The peak frequency in hertz, resolved to 0.02 Hz.
    double peak_frequency(const std::vector<double>& impulse, double sample_rate, double low,
                          double high)
    {
        auto scan = [&](double from, double to, double step)
        {
            double best_frequency = from;
            double best_magnitude = -1.0;
            for (double frequency = from; frequency <= to; frequency += step)
            {
                const double magnitude = magnitude_at(impulse, frequency, sample_rate);
                if (magnitude > best_magnitude)
                {
                    best_magnitude = magnitude;
                    best_frequency = frequency;
                }
            }
            return best_frequency;
        };

        const double coarse = scan(low, high, 2.0);
        return scan(coarse - 4.0, coarse + 4.0, 0.02);
    }
} // namespace

/// @brief Tests that every DkCircuit shape D6c and D6d need compiles, prepares and steps.
/// @see tests/foundation/math/dk/test_dk_circuit/README.md
TEST(Small_DkCircuit, DkCircuitInstantiatesEveryShapeD6cAndD6dNeed)
{
    static constexpr double K_SAMPLE_RATE = 48000.0;

    {
        // Diode-clipper shape: source - R - node - nonlinear - ground.
        SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 2.2e3);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);

        SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

        LinearPort<1> port;
        std::array<double, 1> inputs{0.5};
        std::array<double, 1> outputs{};
        circuit.process_sample(port, inputs, outputs);

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 1u);
        EXPECT_EQ(circuit.non_converged_samples(), 0u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_TRUE(std::isfinite(circuit.nonlinear_currents()[0]));
        EXPECT_TRUE(std::isfinite(circuit.nonlinear_voltages()[0]));
        EXPECT_EQ(circuit.states().size(), 0u);
        EXPECT_LE(circuit.maximum_iterations_seen(), 8u);

        circuit.set_state_limit(1e3);
        circuit.reset_statistics();
        circuit.reset();
        EXPECT_EQ(circuit.total_samples(), 0u);
    }

    {
        // Purely linear shape: two cascaded RC sections, no Newton path at all.
        SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 1.0e4);
        netlist.capacitor(2, 0, 1.0e-8);
        netlist.resistor(2, 3, 1.0e5);
        netlist.capacitor(3, 0, 1.0e-9);
        netlist.output(3, 0);

        SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

        LinearPort<0> port;
        std::array<double, 1> inputs{1.0};
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 8; ++sample)
        {
            circuit.process_sample(port, inputs, outputs);
        }

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 8u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_EQ(circuit.nonlinear_currents().size(), 0u);
        EXPECT_EQ(circuit.nonlinear_voltages().size(), 0u);
        EXPECT_TRUE(std::isfinite(circuit.states()[0]));
        circuit.reset();
    }

    {
        // This is the widest shape D6d instantiates. See tests/foundation/math/dk/test_dk_circuit/README.md.
        SushiDSP::dk::DkNetlist<4, 2, 2, 1> netlist;
        netlist.node_count = 5;
        netlist.voltage_source(1, 0);
        netlist.voltage_source(4, 0);
        netlist.resistor(1, 2, 1.0e4);
        netlist.capacitor(1, 2, 1.0e-10);
        netlist.capacitor(2, 0, 1.0e-8);
        netlist.nonlinear(2, 0);
        netlist.resistor(2, 3, 1.0e5);
        netlist.capacitor(3, 0, 1.0e-9);
        netlist.nonlinear(3, 0);
        netlist.resistor(3, 4, 1.0e4);
        netlist.capacitor(3, 4, 1.0e-9);
        const std::size_t volume = netlist.potentiometer(3, 5, 0, 1.0e5);
        netlist.output(5, 0);

        SushiDSP::dk::DkCircuit<4, 2, 2, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.5);
        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.25);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 1.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());

        LinearPort<2> port;
        std::array<double, 2> inputs{0.1, 250.0};
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 8; ++sample)
        {
            circuit.process_sample(port, inputs, outputs);
        }

        EXPECT_TRUE(std::isfinite(outputs[0]));
        EXPECT_EQ(circuit.total_samples(), 8u);
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
        EXPECT_EQ(circuit.non_converged_samples(), 0u);
        for (std::size_t index = 0; index < 4; ++index)
        {
            EXPECT_TRUE(std::isfinite(circuit.states()[index]));
        }
        circuit.set_state_limit(1e4);
        circuit.reset_statistics();
        circuit.reset();
    }
}

TEST(Small_DkCircuit, DkCircuitRcLowpassMatchesTheBilinearTransformExactly)
{
    // Trapezoidal DK reproduces the bilinear transform. See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr double K_RESISTANCE = 1000.0;
    constexpr double K_CAPACITANCE = 100e-9;
    static constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr double K_EXPECTED_POLE = 0.811320754717;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, K_RESISTANCE);
    netlist.capacitor(2, 0, K_CAPACITANCE);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    // Reference: the bilinear-transform difference equation, written out independently.
    const double conductance = 2.0 * K_CAPACITANCE * K_SAMPLE_RATE;
    const double a0 = 1.0 + K_RESISTANCE * conductance;
    const double a1 = 1.0 - K_RESISTANCE * conductance;
    ASSERT_NEAR(-a1 / a0, K_EXPECTED_POLE, 1e-12);

    double reference_input = 0.0;
    double reference_output = 0.0;

    LinearPort<0> nonlinear;

    for (int sample = 0; sample < 4096; ++sample)
    {
        const double input = std::sin(2.0 * K_PI * 500.0 * sample / K_SAMPLE_RATE) +
                             0.3 * std::sin(2.0 * K_PI * 7000.0 * sample / K_SAMPLE_RATE);
        std::array<double, 1> outputs{};
        std::array<double, 1> inputs{input};
        circuit.process_sample(nonlinear, inputs, outputs);
        reference_output = (input + reference_input - a1 * reference_output) / a0;
        reference_input = input;
        ASSERT_NEAR(outputs[0], reference_output, 1e-12) << sample;
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
    EXPECT_EQ(circuit.non_converged_samples(), 0u);
}

TEST(Small_DkCircuit, DkCircuitSeriesRlcResonatesWhereTheAnalogueCircuitDoes)
{
    // The magnitude peak sits at w0*sqrt(1 - 2*zeta^2). See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr double K_RESISTANCE = 100.0;
    constexpr double K_INDUCTANCE = 10e-3;
    constexpr double K_CAPACITANCE = 100e-9;
    constexpr double K_SAMPLE_RATE = 384000.0;
    constexpr double K_EXPECTED_PEAK_HZ = 4905.485;
    constexpr double K_EXPECTED_PEAK_MAGNITUDE = 3.202563;

    SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, K_RESISTANCE);
    netlist.inductor(2, 3, K_INDUCTANCE);
    netlist.capacitor(3, 0, K_CAPACITANCE);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    // 4096 samples are 53 time constants of the envelope. See tests/foundation/math/dk/test_dk_circuit/README.md.
    LinearPort<0> nonlinear;
    std::vector<double> impulse(4096, 0.0);
    for (std::size_t sample = 0; sample < impulse.size(); ++sample)
    {
        std::array<double, 1> inputs{sample == 0 ? 1.0 : 0.0};
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, inputs, outputs);
        impulse[sample] = outputs[0];
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);

    // DC gain of a series RLC probed across C is exactly 1: no current flows at DC.
    EXPECT_NEAR(magnitude_at(impulse, 0.0, K_SAMPLE_RATE), 1.0, 1e-9);

    const double measured_peak_hz = peak_frequency(impulse, K_SAMPLE_RATE, 4000.0, 6000.0);
    EXPECT_NEAR(measured_peak_hz, K_EXPECTED_PEAK_HZ, 0.005 * K_EXPECTED_PEAK_HZ);
    EXPECT_NEAR(magnitude_at(impulse, measured_peak_hz, K_SAMPLE_RATE), K_EXPECTED_PEAK_MAGNITUDE,
                0.005 * K_EXPECTED_PEAK_MAGNITUDE);
}

TEST(Small_DkCircuit, DkCircuitDiodeClipperReachesTheTrueRootOfTheDiodeEquation)
{
    // The oracle is the bisected root of the diode equation. See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr double K_SOURCE_RESISTANCE = 2200.0;
    constexpr double K_SATURATION_CURRENT = 2.52e-9;
    constexpr double K_THERMAL_VOLTAGE = 0.025864;
    static constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr int K_SAMPLE_COUNT = 2048;

    SushiDSP::dk::DkNetlist<0, 2, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, K_SOURCE_RESISTANCE);
    netlist.nonlinear(2, 0);
    netlist.nonlinear(0, 2);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<0, 2, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    AntiparallelDiodePort port{
        SushiDSP::SiliconDiode(K_SATURATION_CURRENT, 1.0, K_THERMAL_VOLTAGE)};

    // |residual| / slope bounds the distance to the root. See tests/foundation/math/dk/test_dk_circuit/README.md.
    auto residual = [&](double v, double source)
    {
        return K_SOURCE_RESISTANCE * K_SATURATION_CURRENT *
                   (std::exp(v / K_THERMAL_VOLTAGE) - std::exp(-v / K_THERMAL_VOLTAGE)) +
               v - source;
    };
    auto slope = [&](double v)
    {
        return K_SOURCE_RESISTANCE * K_SATURATION_CURRENT / K_THERMAL_VOLTAGE *
                   (std::exp(v / K_THERMAL_VOLTAGE) + std::exp(-v / K_THERMAL_VOLTAGE)) +
               1.0;
    };

    double worst_dk_error = 0.0;

    for (int sample = 0; sample < K_SAMPLE_COUNT; ++sample)
    {
        const double position = static_cast<double>(sample) / (K_SAMPLE_COUNT - 1);
        const double amplitude = 0.05 + position * (8.0 - 0.05);
        const double source = amplitude * std::sin(2.0 * K_PI * 220.0 * sample / K_SAMPLE_RATE);

        std::array<double, 1> inputs{source};
        std::array<double, 1> outputs{};
        circuit.process_sample(port, inputs, outputs);

        // The DK block must land on the true root of the equation, every sample.
        const double dk_error = std::abs(residual(outputs[0], source)) / slope(outputs[0]);
        ASSERT_LT(dk_error, K_DIODE_SOLUTION_ERROR_VOLTS) << sample;
        worst_dk_error = std::max(worst_dk_error, dk_error);
    }

    EXPECT_LT(worst_dk_error, K_DIODE_SOLUTION_ERROR_VOLTS);
}

TEST(Small_DkCircuit, DkCircuitPotentiometerChangesTheResponseAndCostsNothingWhenUnchanged)
{
    // The DC gain from source to wiper is the position. See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr double K_TRACK_OHMS = 10000.0;
    constexpr double K_CAPACITANCE = 100e-9;
    static constexpr double K_SAMPLE_RATE = 48000.0;
    // tau is at most 250 us, so 40000 samples settle it. See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr int K_SETTLE_SAMPLES = 40000;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    const std::size_t volume = netlist.potentiometer(0, 2, 1, K_TRACK_OHMS);
    netlist.capacitor(2, 0, K_CAPACITANCE);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    LinearPort<0> nonlinear;
    std::array<double, 1> inputs{1.0};
    std::array<double, 1> outputs{};

    auto settled_gain = [&](double position)
    {
        circuit.set_potentiometer(volume, position);
        circuit.update_matrices_if_dirty();
        circuit.reset();
        for (int sample = 0; sample < K_SETTLE_SAMPLES; ++sample)
        {
            circuit.process_sample(nonlinear, inputs, outputs);
        }
        return outputs[0];
    };

    EXPECT_NEAR(settled_gain(0.25), 0.25, 1e-9);
    EXPECT_NEAR(settled_gain(0.5), 0.5, 1e-9);
    EXPECT_NEAR(settled_gain(0.9), 0.9, 1e-9);

    // K_MINIMUM_POT_OHMS floors a shorted section at 1 Ohm. See tests/foundation/math/dk/test_dk_circuit/README.md.
    const double bottom = settled_gain(0.0);
    const double top = settled_gain(1.0);
    EXPECT_TRUE(std::isfinite(bottom));
    EXPECT_TRUE(std::isfinite(top));
    EXPECT_NEAR(bottom, 1.0 / 10001.0, 1e-9);
    EXPECT_NEAR(top, 10000.0 / 10001.0, 1e-9);

    circuit.set_potentiometer(volume, 0.5);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());  // moved off the 1.0 end stop
    EXPECT_FALSE(circuit.update_matrices_if_dirty()); // nothing moved
    circuit.set_potentiometer(volume, 0.5);
    EXPECT_FALSE(circuit.update_matrices_if_dirty()); // same value, still not dirty
    circuit.set_potentiometer(volume, 0.9);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());
}

TEST(Small_DkCircuit, DkCircuitNeverEmitsANonFiniteSampleAndCountsItsFallbacks)
{
    // One inf sample must not poison the capacitor's state. See tests/foundation/math/dk/test_dk_circuit/README.md.
    static constexpr double K_RESISTANCE = 1000.0;
    static constexpr double K_CAPACITANCE = 100e-9;
    static constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr int K_POISONED_SAMPLE = 64;

    auto build = [](SushiDSP::dk::DkCircuit<1, 1, 1, 1>& circuit)
    {
        SushiDSP::dk::DkNetlist<1, 1, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, K_RESISTANCE);
        netlist.capacitor(2, 0, K_CAPACITANCE);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);
        return circuit.prepare(netlist, K_SAMPLE_RATE);
    };

    SushiDSP::dk::DkCircuit<1, 1, 1, 1> circuit;
    SushiDSP::dk::DkCircuit<1, 1, 1, 1> reference;
    ASSERT_TRUE(build(circuit));
    ASSERT_TRUE(build(reference));

    PoisonablePort port;
    PoisonablePort healthy;

    double deviation_after_the_fallback = 0.0;
    double deviation_at_the_end = 0.0;

    for (int sample = 0; sample < 256; ++sample)
    {
        const double source = std::sin(2.0 * K_PI * 300.0 * sample / K_SAMPLE_RATE);
        std::array<double, 1> inputs{source};
        std::array<double, 1> outputs{};
        std::array<double, 1> reference_outputs{};

        port.poisoned = sample == K_POISONED_SAMPLE;
        circuit.process_sample(port, inputs, outputs);
        reference.process_sample(healthy, inputs, reference_outputs);

        EXPECT_TRUE(std::isfinite(outputs[0])) << sample;
        if (sample < K_POISONED_SAMPLE)
        {
            // Before the poisoned sample the two are the same computation, bit for bit.
            EXPECT_NEAR(outputs[0], reference_outputs[0], 1e-15) << sample;
        }
        else if (sample == K_POISONED_SAMPLE + 1)
        {
            deviation_after_the_fallback = std::abs(outputs[0] - reference_outputs[0]);
        }
        else if (sample == 255)
        {
            deviation_at_the_end = std::abs(outputs[0] - reference_outputs[0]);
        }
    }

    // The fallback costs one sample's worth of state update. See tests/foundation/math/dk/test_dk_circuit/README.md.
    EXPECT_LT(deviation_after_the_fallback, 0.05);
    EXPECT_GT(deviation_after_the_fallback, 0.0);
    EXPECT_LT(deviation_at_the_end, 1e-15);

    EXPECT_EQ(circuit.nan_fallback_samples(), 1u);
    EXPECT_EQ(reference.nan_fallback_samples(), 0u);
}

TEST(Small_DkCircuit, DkCircuitClampsItsStateUnderAnAbsurdInputAndStaysFinite)
{
    // The output stays finite and the state stops at a limit. See tests/foundation/math/dk/test_dk_circuit/README.md.
    constexpr double K_RESISTANCE = 1000.0;
    constexpr double K_CAPACITANCE = 100e-9;
    static constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr double K_STATE_LIMIT = 1000.0;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, K_RESISTANCE);
    netlist.capacitor(2, 0, K_CAPACITANCE);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));
    circuit.set_state_limit(K_STATE_LIMIT);

    LinearPort<0> nonlinear;
    std::array<double, 1> inputs{1e30};
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 64; ++sample)
    {
        circuit.process_sample(nonlinear, inputs, outputs);
        ASSERT_TRUE(std::isfinite(outputs[0])) << sample;
        ASSERT_LE(std::abs(circuit.states()[0]), K_STATE_LIMIT) << sample;
    }
    EXPECT_NEAR(std::abs(circuit.states()[0]), K_STATE_LIMIT, 1e-9);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}
