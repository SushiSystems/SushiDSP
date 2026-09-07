/**************************************************************************/
/* test_dk_circuit.cpp                                                    */
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

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"

#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

namespace
{
    /// A deliberately trivial nonlinear port: a linear conductance. It exercises the glue
    /// contract and the Newton path without depending on any circuit model, which keeps this
    /// instantiation gate independent of Tasks 2 to 4.
    template <std::size_t N>
    struct LinearPort
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
    constexpr double kDiodeSolutionErrorVolts = 1e-11;

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double kPi = 3.14159265358979323846;

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
        double conductance = 1e-4;  ///< Branch conductance while healthy.
        bool   poisoned = false;    ///< When set, the branch current is infinity.

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

    /// @brief Magnitude of the DTFT of a real impulse response at one frequency.
    ///
    /// |H(e^{jw})| = |sum_n h[n] e^{-jwn}|. Evaluated directly rather than by FFT because the
    /// resonance test needs magnitudes on an arbitrarily fine frequency grid, at full double
    /// precision, and the impulse response is short.
    /// @param impulse The impulse response, assumed fully decayed by its last sample.
    /// @param frequency_hz Frequency to evaluate at.
    /// @param sample_rate Sample rate the response was measured at.
    /// @return The magnitude, a pure number.
    double magnitude_at(const std::vector<double>& impulse, double frequency_hz, double sample_rate)
    {
        const double omega = 2.0 * kPi * frequency_hz / sample_rate;
        double       real = 0.0;
        double       imaginary = 0.0;
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
    double peak_frequency(const std::vector<double>& impulse, double sample_rate, double low, double high)
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
}

// Compile-and-smoke gate for the three DkCircuit shapes D6c and D6d instantiate. Tasks 6 and 7
// own the numerical validation; this case only proves every shape compiles, prepares and steps.
TEST(Unit_Dsp, DkCircuitInstantiatesEveryShapeD6cAndD6dNeed)
{
    constexpr double kSampleRate = 48000.0;

    {
        // Diode-clipper shape: source - R - node - nonlinear - ground.
        SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 2.2e3);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);

        SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        LinearPort<1>         port;
        std::array<double, 1> inputs{ 0.5 };
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
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        LinearPort<0>         port;
        std::array<double, 1> inputs{ 1.0 };
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
        // The widest D6d-style shape: four states, a two-branch nonlinear port, two sources
        // (signal plus supply rail), one probe, and a potentiometer to drive re-derivation.
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
        ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.5);
        EXPECT_FALSE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.25);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 0.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());
        circuit.set_potentiometer(volume, 1.0);
        EXPECT_TRUE(circuit.update_matrices_if_dirty());

        LinearPort<2>         port;
        std::array<double, 2> inputs{ 0.1, 250.0 };
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

TEST(Unit_Dsp, DkCircuitRcLowpassMatchesTheBilinearTransformExactly)
{
    // Node 1 = source, node 2 = output. R from 1 to 2, C from 2 to ground, probe node 2.
    // Trapezoidal DK must reproduce the bilinear transform of 1/(1 + sRC) exactly:
    //   Gc = 2C/T,  H(z) = (1 + z^-1) / ((1 + R*Gc) + (1 - R*Gc) z^-1)
    // With R = 1 kOhm, C = 100 nF, fs = 48000: Gc = 2*1e-7*48000 = 9.6e-3 S and R*Gc = 9.6, so
    //   pole      = -a1/a0 = (R*Gc - 1)/(R*Gc + 1) = 8.6/10.6 = +0.811320754717
    //   DC gain   = (1 + 1)/((1 + 9.6) + (1 - 9.6)) = 2/2 = 1 exactly
    //   Nyquist   = (1 - 1)/(...) = 0 exactly
    // Note the sign: a lowpass pole must approach +1 as fs -> infinity, so the pole is
    // (R*Gc - 1)/(R*Gc + 1) and NOT (1 - R*Gc)/(1 + R*Gc); the latter is a Nyquist pole. The
    // -3 dB point of the analogue prototype is 1/(2*pi*R*C) = 1591.549431 Hz.
    constexpr double kResistance = 1000.0;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 48000.0;
    constexpr double kExpectedPole = 0.811320754717;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kResistance);
    netlist.capacitor(2, 0, kCapacitance);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    // Reference: the bilinear-transform difference equation, written out independently.
    const double conductance = 2.0 * kCapacitance * kSampleRate;
    const double a0 = 1.0 + kResistance * conductance;
    const double a1 = 1.0 - kResistance * conductance;
    ASSERT_NEAR(-a1 / a0, kExpectedPole, 1e-12);

    double reference_input = 0.0;
    double reference_output = 0.0;

    LinearPort<0> nonlinear;

    for (int sample = 0; sample < 4096; ++sample)
    {
        const double input = std::sin(2.0 * kPi * 500.0 * sample / kSampleRate)
                             + 0.3 * std::sin(2.0 * kPi * 7000.0 * sample / kSampleRate);
        std::array<double, 1> outputs{};
        std::array<double, 1> inputs{ input };
        circuit.process_sample(nonlinear, inputs, outputs);
        reference_output = (input + reference_input - a1 * reference_output) / a0;
        reference_input = input;
        ASSERT_NEAR(outputs[0], reference_output, 1e-12) << sample;
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
    EXPECT_EQ(circuit.non_converged_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitSeriesRlcResonatesWhereTheAnalogueCircuitDoes)
{
    // Source -> R -> L -> C -> ground, probing across C: H(s) = 1 / (1 + sRC + s^2 LC).
    // With R = 100 Ohm, L = 10 mH, C = 100 nF:
    //   w0    = 1/sqrt(LC)            = sqrt(1e9)                 = 31622.776602 rad/s
    //                                                             -> f0   = 5032.921210 Hz
    //   zeta  = (R/2)*sqrt(C/L)       = 50*sqrt(1e-5)             = 0.158113883, zeta^2 = 0.025
    //   w_d   = w0*sqrt(1 - zeta^2)   = sqrt(1/(LC) - R^2/(4L^2)) = 31224.989990 rad/s
    //                                                             -> f_d  = 4969.611 Hz
    //   w_pk  = w0*sqrt(1 - 2*zeta^2) = sqrt(1/(LC) - R^2/(2L^2)) = 30822.070014 rad/s
    //                                                             -> f_pk = 4905.485 Hz
    // The MAGNITUDE peak of a second-order lowpass sits at w0*sqrt(1 - 2*zeta^2), not at the
    // damped natural frequency w0*sqrt(1 - zeta^2); the two differ by 1.3 % here, well outside
    // the 0.5 % tolerance, so the damped resonance is the wrong golden for a measured peak.
    // Peak magnitude is 1/(2*zeta*sqrt(1 - zeta^2)) = 3.202563.
    //
    // Run rate 384 kHz so trapezoidal frequency warping is negligible: the digital peak sits at
    // (2/T)*atan(w_pk*T/2) = 30805.6 rad/s, 0.053 % below the analogue peak - an order of
    // magnitude inside the tolerance, so the assertion is about the circuit, not the warping.
    //
    // The response is measured as the DTFT of the impulse response rather than with
    // measure::frequency_response: the resonance is broad (Q = 3.16), so a 0.5 % frequency error
    // costs only 0.004 dB of magnitude. Locating the peak by argmax therefore needs a magnitude
    // accuracy far below the ~0.05 dB a swept-sine measurement delivers, and a sweep-based
    // assertion here would be measuring the sweep rather than the circuit.
    constexpr double kResistance = 100.0;
    constexpr double kInductance = 10e-3;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 384000.0;
    constexpr double kExpectedPeakHz = 4905.485;
    constexpr double kExpectedPeakMagnitude = 3.202563;

    SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
    netlist.node_count = 3;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kResistance);
    netlist.inductor(2, 3, kInductance);
    netlist.capacitor(3, 0, kCapacitance);
    netlist.output(3, 0);

    SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    // The envelope decays as exp(-zeta*w0*t), one time constant every 76.8 samples at 384 kHz,
    // so 4096 samples is 53 time constants: the truncated tail is below 1e-23 of the peak.
    LinearPort<0>       nonlinear;
    std::vector<double> impulse(4096, 0.0);
    for (std::size_t sample = 0; sample < impulse.size(); ++sample)
    {
        std::array<double, 1> inputs{ sample == 0 ? 1.0 : 0.0 };
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, inputs, outputs);
        impulse[sample] = outputs[0];
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);

    // DC gain of a series RLC probed across C is exactly 1: no current flows at DC.
    EXPECT_NEAR(magnitude_at(impulse, 0.0, kSampleRate), 1.0, 1e-9);

    const double measured_peak_hz = peak_frequency(impulse, kSampleRate, 4000.0, 6000.0);
    EXPECT_NEAR(measured_peak_hz, kExpectedPeakHz, 0.005 * kExpectedPeakHz);
    EXPECT_NEAR(magnitude_at(impulse, measured_peak_hz, kSampleRate), kExpectedPeakMagnitude,
                0.005 * kExpectedPeakMagnitude);
}

TEST(Unit_Dsp, DkCircuitDiodeClipperReachesTheTrueRootOfTheDiodeEquation)
{
    // The circuit solves v = Vs - Rs*Is*(exp(v/Vt) - exp(-v/Vt)). Node 1 = source, node 2 = the
    // diode node, Rs from 1 to 2, and TWO antiparallel SiliconDiode branches from 2 to ground -
    // a pair, not a single symmetric element, so D6c can make it asymmetric later. Using
    // SiliconDiode with n = 1 and the same Vt reproduces that law exactly except for its -1
    // offsets, which cancel between the two antiparallel branches.
    //
    // D6b's DiodeClipperCircuit used to sit here too, as a second, independently written
    // implementation to agree with. D6c's plan (docs/superpowers/plans/2026-09-05-d6c-tubescreamer
    // -on-dk.md, Task 10) retired it: it ran four fixed float Newton iterations with no
    // convergence test and drifted up to 2.8 mV from the root at high slew, so agreeing with it
    // was a weaker claim than agreeing with the equation. Bisecting the scalar equation itself,
    // in double precision, is an independently written *root* rather than an independently
    // written approximation, and it is the oracle this case now compares against exclusively.
    constexpr double kSourceResistance = 2200.0;
    constexpr double kSaturationCurrent = 2.52e-9;
    constexpr double kThermalVoltage = 0.025864;
    constexpr double kSampleRate = 48000.0;
    constexpr int    kSampleCount = 2048;

    SushiDSP::dk::DkNetlist<0, 2, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kSourceResistance);
    netlist.nonlinear(2, 0);
    netlist.nonlinear(0, 2);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<0, 2, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    AntiparallelDiodePort port{ SushiDSP::SiliconDiode(kSaturationCurrent, 1.0, kThermalVoltage) };

    // The scalar equation, evaluated in double precision, and its slope. |residual| over the
    // slope is a first-order bound on the distance from a candidate voltage to the equation's
    // true root, which is the oracle: it compares the DK solution against the equation itself
    // rather than against another approximation of it.
    auto residual = [&](double v, double source)
    {
        return kSourceResistance * kSaturationCurrent
                   * (std::exp(v / kThermalVoltage) - std::exp(-v / kThermalVoltage))
               + v - source;
    };
    auto slope = [&](double v)
    {
        return kSourceResistance * kSaturationCurrent / kThermalVoltage
                   * (std::exp(v / kThermalVoltage) + std::exp(-v / kThermalVoltage))
               + 1.0;
    };

    double worst_dk_error = 0.0;

    for (int sample = 0; sample < kSampleCount; ++sample)
    {
        const double position = static_cast<double>(sample) / (kSampleCount - 1);
        const double amplitude = 0.05 + position * (8.0 - 0.05);
        const double source = amplitude * std::sin(2.0 * kPi * 220.0 * sample / kSampleRate);

        std::array<double, 1> inputs{ source };
        std::array<double, 1> outputs{};
        circuit.process_sample(port, inputs, outputs);

        // The DK block must land on the true root of the equation, every sample.
        const double dk_error = std::abs(residual(outputs[0], source)) / slope(outputs[0]);
        ASSERT_LT(dk_error, kDiodeSolutionErrorVolts) << sample;
        worst_dk_error = std::max(worst_dk_error, dk_error);
    }

    EXPECT_LT(worst_dk_error, kDiodeSolutionErrorVolts);
}

TEST(Unit_Dsp, DkCircuitPotentiometerChangesTheResponseAndCostsNothingWhenUnchanged)
{
    // A 10 kOhm pot as a voltage divider into a 100 nF cap. The track runs from ground (the
    // pot's a terminal) to the source (its b terminal), so the a-to-wiper section is
    // position*10k and the DC gain from the source to the wiper is exactly the position: a DC
    // input of 1 V settles to p at the output. Positions 0.25, 0.5, 0.9 give 0.25, 0.5, 0.9.
    constexpr double kTrackOhms = 10000.0;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 48000.0;
    // The worst-case source impedance seen by the cap is the parallel combination of the two
    // sections, at most 2.5 kOhm, so tau <= 250 us. 40000 samples is 0.83 s, over 3000 tau.
    constexpr int kSettleSamples = 40000;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    const std::size_t volume = netlist.potentiometer(0, 2, 1, kTrackOhms);
    netlist.capacitor(2, 0, kCapacitance);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    LinearPort<0>         nonlinear;
    std::array<double, 1> inputs{ 1.0 };
    std::array<double, 1> outputs{};

    auto settled_gain = [&](double position)
    {
        circuit.set_potentiometer(volume, position);
        circuit.update_matrices_if_dirty();
        circuit.reset();
        for (int sample = 0; sample < kSettleSamples; ++sample)
        {
            circuit.process_sample(nonlinear, inputs, outputs);
        }
        return outputs[0];
    };

    EXPECT_NEAR(settled_gain(0.25), 0.25, 1e-9);
    EXPECT_NEAR(settled_gain(0.5), 0.5, 1e-9);
    EXPECT_NEAR(settled_gain(0.9), 0.9, 1e-9);

    // Both end stops must still derive and produce finite output. kMinimumPotOhms floors the
    // shorted section at 1 Ohm rather than 0, so the divider reads 1/10001 and 10000/10001
    // instead of exactly 0 and 1; that 0.01 % is the floor, and it is what keeps the augmented
    // nodal matrix non-singular at the stops.
    const double bottom = settled_gain(0.0);
    const double top = settled_gain(1.0);
    EXPECT_TRUE(std::isfinite(bottom));
    EXPECT_TRUE(std::isfinite(top));
    EXPECT_NEAR(bottom, 1.0 / 10001.0, 1e-9);
    EXPECT_NEAR(top, 10000.0 / 10001.0, 1e-9);

    circuit.set_potentiometer(volume, 0.5);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());       // moved off the 1.0 end stop
    EXPECT_FALSE(circuit.update_matrices_if_dirty());      // nothing moved
    circuit.set_potentiometer(volume, 0.5);
    EXPECT_FALSE(circuit.update_matrices_if_dirty());      // same value, still not dirty
    circuit.set_potentiometer(volume, 0.9);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());
}

TEST(Unit_Dsp, DkCircuitNeverEmitsANonFiniteSampleAndCountsItsFallbacks)
{
    // An RC lowpass with a linear nonlinear branch across the cap, so there is a state a
    // poisoned sample could corrupt. A nonlinearity that deliberately returns inf on one sample
    // must not poison that state: the trajectory has to return to that of an identical circuit
    // that was never poisoned.
    constexpr double kResistance = 1000.0;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 48000.0;
    constexpr int    kPoisonedSample = 64;

    auto build = [](SushiDSP::dk::DkCircuit<1, 1, 1, 1>& circuit)
    {
        SushiDSP::dk::DkNetlist<1, 1, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, kResistance);
        netlist.capacitor(2, 0, kCapacitance);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);
        return circuit.prepare(netlist, kSampleRate);
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
        const double          source = std::sin(2.0 * kPi * 300.0 * sample / kSampleRate);
        std::array<double, 1> inputs{ source };
        std::array<double, 1> outputs{};
        std::array<double, 1> reference_outputs{};

        port.poisoned = sample == kPoisonedSample;
        circuit.process_sample(port, inputs, outputs);
        reference.process_sample(healthy, inputs, reference_outputs);

        EXPECT_TRUE(std::isfinite(outputs[0])) << sample;
        if (sample < kPoisonedSample)
        {
            // Before the poisoned sample the two are the same computation, bit for bit.
            EXPECT_NEAR(outputs[0], reference_outputs[0], 1e-15) << sample;
        }
        else if (sample == kPoisonedSample + 1)
        {
            deviation_after_the_fallback = std::abs(outputs[0] - reference_outputs[0]);
        }
        else if (sample == 255)
        {
            deviation_at_the_end = std::abs(outputs[0] - reference_outputs[0]);
        }
    }

    // The fallback holds the previous state rather than advancing it, so the poisoned sample
    // costs exactly one sample's worth of state update - a bounded step off the reference
    // trajectory, not a poisoned state. The bound below is the size of one sample's output
    // change on this stimulus (~0.04 V at 300 Hz through a 100 us time constant); measured
    // 0.022. The two then reconverge at the circuit's own pole, 0.811 per sample here, so 190
    // samples later the deviation is 0.811^190 = 6e-18 of that step, i.e. gone. A state that
    // had actually been poisoned would have stayed non-finite, or diverged, instead.
    EXPECT_LT(deviation_after_the_fallback, 0.05);
    EXPECT_GT(deviation_after_the_fallback, 0.0);
    EXPECT_LT(deviation_at_the_end, 1e-15);

    EXPECT_EQ(circuit.nan_fallback_samples(), 1u);
    EXPECT_EQ(reference.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkCircuitClampsItsStateUnderAnAbsurdInputAndStaysFinite)
{
    // 1e30 V into the RC lowpass. The output is allowed to be enormous - it is a linear circuit
    // driven by an enormous source - but it must stay finite, and the state must stop at the
    // configured limit rather than running away.
    constexpr double kResistance = 1000.0;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 48000.0;
    constexpr double kStateLimit = 1000.0;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kResistance);
    netlist.capacitor(2, 0, kCapacitance);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));
    circuit.set_state_limit(kStateLimit);

    LinearPort<0>         nonlinear;
    std::array<double, 1> inputs{ 1e30 };
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 64; ++sample)
    {
        circuit.process_sample(nonlinear, inputs, outputs);
        ASSERT_TRUE(std::isfinite(outputs[0])) << sample;
        ASSERT_LE(std::abs(circuit.states()[0]), kStateLimit) << sample;
    }
    EXPECT_NEAR(std::abs(circuit.states()[0]), kStateLimit, 1e-9);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}
