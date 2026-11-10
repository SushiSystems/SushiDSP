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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
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
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>

#include <SushiDSP/foundation/math/dk/conductance_port.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>
#include <SushiDSP/foundation/math/dk/poisonable_port.hpp>

namespace
{
    const double kU = TestSupport::unit_roundoff<double>();

    /// The conductance of each branch of the linear stand-in port, in siemens: small enough that the
    /// branch barely loads the shapes it is wired into.
    constexpr double K_LINEAR_BRANCH_SIEMENS = 1e-6;

    /// The conductance of the poisonable branch while healthy, in siemens.
    constexpr double K_POISONABLE_BRANCH_SIEMENS = 1e-4;

    /// Pi to double precision, spelled out so the tests do not depend on M_PI being visible.
    constexpr double K_PI = 3.14159265358979323846;

    /// The iteration budget of the circuit's solver as shipped.
    constexpr std::uint32_t K_DEFAULT_ITERATION_BUDGET = 8;

    /// The relative step limit of the circuit's solver as shipped: it stops once a step is below this times (1 + |v|).
    constexpr double K_SOLVER_STEP_TOLERANCE = 1e-9;

    /// The absolute residual limit of the circuit's solver as shipped: it stops at once when the residual's largest entry, in volts for a DK port, is below this.
    constexpr double K_SOLVER_RESIDUAL_TOLERANCE = 1e-12;

    /// The sample rate of every circuit here unless a test names another.
    constexpr double K_SAMPLE_RATE = 48000.0;

    using LowpassCircuit = SushiDSP::dk::DkCircuit<1, 0, 1, 1>;
    using LowpassNetlist = SushiDSP::dk::DkNetlist<1, 0, 1, 1>;

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

    /// @brief Builds source - R - node - nonlinear - ground, the diode-clipper shape.
    void build_clipper_shape(SushiDSP::dk::DkNetlist<0, 1, 1, 1>& netlist)
    {
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 2.2e3);
        netlist.nonlinear(2, 0);
        netlist.output(2, 0);
    }

    /// @brief Builds two cascaded RC sections, a purely linear shape with no Newton path.
    void build_cascade_shape(SushiDSP::dk::DkNetlist<2, 0, 1, 1>& netlist)
    {
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 1.0e4);
        netlist.capacitor(2, 0, 1.0e-8);
        netlist.resistor(2, 3, 1.0e5);
        netlist.capacitor(3, 0, 1.0e-9);
        netlist.output(3, 0);
    }

    /// @brief Builds the widest shape D6d instantiates and returns the index of its potentiometer.
    /// See tests/foundation/math/dk/test_dk_circuit/README.md.
    std::size_t build_widest_shape(SushiDSP::dk::DkNetlist<4, 2, 2, 1>& netlist)
    {
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
        return volume;
    }

    /// @brief Builds an RC lowpass of 1 kOhm and 100 nF.
    void build_rc_lowpass(LowpassNetlist& netlist)
    {
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 1000.0);
        netlist.capacitor(2, 0, 100e-9);
        netlist.output(2, 0);
    }

    /// @brief Builds an inverting ideal op-amp lowpass: 10 kOhm in, 100 kOhm and 10 nF in feedback.
    void build_op_amp_lowpass(LowpassNetlist& netlist)
    {
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, 10.0e3);
        netlist.resistor(2, 3, 100.0e3);
        netlist.capacitor(2, 3, 10.0e-9);
        netlist.nullor(0, 2, 3, 0);
        netlist.output(3, 0);
    }

    /// @brief Holds the difference equation y[n] = (b0 x[n] + b1 x[n-1] - a1 y[n-1]) / a0 of a first-order section.
    struct FirstOrderSection
    {
            double b0;
            double b1;
            double a0;
            double a1;
    };

    /// @brief Holds one bilinear row: a lowpass netlist, its reference section and the two tones that drive it.
    ///
    /// The trapezoidal rule on a first-order circuit is its bilinear transform, so the reference is the
    /// section the circuit's component values give: the capacitor becomes the conductance 2 C fs.
    struct BilinearRow
    {
            const char* name;
            void (*build)(LowpassNetlist&);
            FirstOrderSection section;
            double low_tone_hz;
            double high_tone_hz;
    };

    constexpr double K_RC_ALPHA = 2.0 * 100e-9 * K_SAMPLE_RATE * 1000.0;
    constexpr double K_OP_AMP_ALPHA = 2.0 * K_SAMPLE_RATE * 100.0e3 * 10.0e-9;
    constexpr double K_OP_AMP_GAIN = 100.0e3 / 10.0e3;

    // RC: pole 43/53. Op-amp: pole 95/97 and a DC gain of -10, exactly.
    const BilinearRow kBilinearRows[] = {
        {"RcLowpass", build_rc_lowpass, {1.0, 1.0, 1.0 + K_RC_ALPHA, 1.0 - K_RC_ALPHA}, 500.0, 7000.0},
        {"OpAmpLowpass",
         build_op_amp_lowpass,
         {-K_OP_AMP_GAIN, -K_OP_AMP_GAIN, 1.0 + K_OP_AMP_ALPHA, 1.0 - K_OP_AMP_ALPHA},
         137.0,
         6000.0},
    };

    /// @brief Holds the outcome of a run of a lowpass against its reference section.
    struct BilinearRun
    {
            TestSupport::ErrorRatio worst;
            std::uint64_t fallbacks = 0;
            std::uint64_t non_converged = 0;
    };

    constexpr int K_BILINEAR_SAMPLES = 4096;

    /// @brief Drives a lowpass and its reference section with the same two-tone input and keeps the worst error ratio.
    ///
    /// Each output differs from the reference by the rounding of one update, about 6 u times the output peak, and
    /// the error of the previous sample reaches it through the pole p, so the sum is bounded by 6 u peak / (1 - |p|).
    BilinearRun run_bilinear(const BilinearRow& row)
    {
        LowpassNetlist netlist;
        row.build(netlist);
        LowpassCircuit circuit;
        BilinearRun run;
        if (!circuit.prepare(netlist, K_SAMPLE_RATE))
        {
            run.worst.ratio = std::numeric_limits<double>::infinity();
            return run;
        }

        std::vector<double> reference(K_BILINEAR_SAMPLES);
        std::vector<double> produced(K_BILINEAR_SAMPLES);
        SushiDSP::dk::LinearPort nonlinear;
        double previous_input = 0.0;
        double previous_output = 0.0;
        double peak = 0.0;
        for (int sample = 0; sample < K_BILINEAR_SAMPLES; ++sample)
        {
            const double input = std::sin(2.0 * K_PI * row.low_tone_hz * sample / K_SAMPLE_RATE) +
                                 0.3 * std::sin(2.0 * K_PI * row.high_tone_hz * sample / K_SAMPLE_RATE);
            std::array<double, 1> outputs{};
            circuit.process_sample(nonlinear, std::array<double, 1>{input}, outputs);
            produced[sample] = outputs[0];
            previous_output = (row.section.b0 * input + row.section.b1 * previous_input - row.section.a1 * previous_output) /
                              row.section.a0;
            previous_input = input;
            reference[sample] = previous_output;
            peak = std::max(peak, std::fabs(previous_output));
        }

        const double pole = std::fabs(row.section.a1 / row.section.a0);
        const double bound = 6.0 * kU * peak / (1.0 - pole);
        for (int sample = 0; sample < K_BILINEAR_SAMPLES; ++sample)
        {
            const TestSupport::ErrorRatio measured = TestSupport::measure_error(produced[sample], reference[sample], bound);
            if (measured.ratio > run.worst.ratio || std::isnan(measured.ratio))
            {
                run.worst = measured;
            }
        }
        run.fallbacks = circuit.nan_fallback_samples();
        run.non_converged = circuit.non_converged_samples();
        return run;
    }

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

    /// @brief Holds the finest step of the peak scan, in hertz.
    constexpr double K_PEAK_SCAN_STEP_HZ = 0.02;

    /// @brief Locates the frequency of the largest magnitude by a two-stage grid scan.
    /// @param impulse The impulse response.
    /// @param sample_rate Sample rate the response was measured at.
    /// @param low Lower bound of the coarse search, scanned in 2 Hz steps.
    /// @param high Upper bound of the coarse search.
    /// @return The peak frequency in hertz, resolved to K_PEAK_SCAN_STEP_HZ.
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
        return scan(coarse - 4.0, coarse + 4.0, K_PEAK_SCAN_STEP_HZ);
    }

    /// @brief Holds the series RLC probed across its capacitor, and the analytic response it must reproduce.
    ///
    /// With w0 = 1 / sqrt(L C) and zeta = (R / 2) sqrt(C / L) the analogue response peaks at
    /// w_pk = w0 sqrt(1 - 2 zeta^2) with magnitude 1 / (2 zeta sqrt(1 - zeta^2)). Trapezoidal integration maps
    /// an analogue frequency W to the digital frequency (2 / T) atan(W T / 2) and leaves every magnitude
    /// unchanged, so the digital peak sits at f = (fs / pi) atan(w_pk / (2 fs)) with the same height.
    struct SeriesRlc
    {
            static constexpr double K_RESISTANCE = 100.0;
            static constexpr double K_INDUCTANCE = 10e-3;
            static constexpr double K_CAPACITANCE = 100e-9;
            static constexpr double K_RLC_SAMPLE_RATE = 384000.0;
            static constexpr std::size_t K_IMPULSE_LENGTH = 4096;

            std::vector<double> impulse;
            std::uint64_t fallbacks = 0;

            static double zeta()
            {
                return 0.5 * K_RESISTANCE * std::sqrt(K_CAPACITANCE / K_INDUCTANCE);
            }

            static double analogue_peak_radians()
            {
                return std::sqrt(1.0 / (K_INDUCTANCE * K_CAPACITANCE)) * std::sqrt(1.0 - 2.0 * zeta() * zeta());
            }

            static double expected_peak_hz()
            {
                return K_RLC_SAMPLE_RATE / K_PI * std::atan(analogue_peak_radians() / (2.0 * K_RLC_SAMPLE_RATE));
            }

            static double expected_peak_magnitude()
            {
                return 1.0 / (2.0 * zeta() * std::sqrt(1.0 - zeta() * zeta()));
            }
    };

    /// @brief Steps the series RLC with a unit impulse and keeps the response.
    /// 4096 samples are 53 time constants of the envelope. See tests/foundation/math/dk/test_dk_circuit/README.md.
    SeriesRlc measure_series_rlc()
    {
        SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
        netlist.node_count = 3;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, SeriesRlc::K_RESISTANCE);
        netlist.inductor(2, 3, SeriesRlc::K_INDUCTANCE);
        netlist.capacitor(3, 0, SeriesRlc::K_CAPACITANCE);
        netlist.output(3, 0);

        SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
        SeriesRlc result;
        if (!circuit.prepare(netlist, SeriesRlc::K_RLC_SAMPLE_RATE))
        {
            return result;
        }
        SushiDSP::dk::LinearPort nonlinear;
        result.impulse.assign(SeriesRlc::K_IMPULSE_LENGTH, 0.0);
        for (std::size_t sample = 0; sample < result.impulse.size(); ++sample)
        {
            std::array<double, 1> outputs{};
            circuit.process_sample(nonlinear, std::array<double, 1>{sample == 0 ? 1.0 : 0.0}, outputs);
            result.impulse[sample] = outputs[0];
        }
        result.fallbacks = circuit.nan_fallback_samples();
        return result;
    }

    constexpr double K_SOURCE_RESISTANCE = 2200.0;
    constexpr double K_SATURATION_CURRENT = 2.52e-9;
    constexpr double K_THERMAL_VOLTAGE = 0.025864;
    constexpr int K_CLIPPER_SAMPLES = 2048;

    /// @brief Steps the antiparallel-diode clipper over a rising sweep and returns the worst distance to the root.
    ///
    /// |residual| / slope bounds the distance to the root to first order. After a step below K_SOLVER_STEP_TOLERANCE
    /// (1 + |v|) Newton's error is at most f'' / (2 f') = 1 / (2 Vt) times that step squared; a residual below
    /// K_SOLVER_RESIDUAL_TOLERANCE ends the solve at most that tolerance over the slope (at least 1) from the root.
    /// @param prepared Receives whether the circuit prepared.
    /// @return The largest ratio of an error to its bound over the sweep.
    double worst_clipper_error_ratio(bool& prepared)
    {
        SushiDSP::dk::DkNetlist<0, 2, 1, 1> netlist;
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        netlist.resistor(1, 2, K_SOURCE_RESISTANCE);
        netlist.nonlinear(2, 0);
        netlist.nonlinear(0, 2);
        netlist.output(2, 0);

        SushiDSP::dk::DkCircuit<0, 2, 1, 1> circuit;
        prepared = circuit.prepare(netlist, K_SAMPLE_RATE);
        AntiparallelDiodePort port{SushiDSP::SiliconDiode(K_SATURATION_CURRENT, 1.0, K_THERMAL_VOLTAGE)};

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

        double worst = 0.0;
        for (int sample = 0; sample < K_CLIPPER_SAMPLES; ++sample)
        {
            const double position = static_cast<double>(sample) / (K_CLIPPER_SAMPLES - 1);
            const double amplitude = 0.05 + position * (8.0 - 0.05);
            const double source = amplitude * std::sin(2.0 * K_PI * 220.0 * sample / K_SAMPLE_RATE);

            std::array<double, 1> outputs{};
            circuit.process_sample(port, std::array<double, 1>{source}, outputs);

            const double step = K_SOLVER_STEP_TOLERANCE * (1.0 + std::fabs(outputs[0]));
            const double error = std::fabs(residual(outputs[0], source)) / slope(outputs[0]);
            const double bound = step * step / (2.0 * K_THERMAL_VOLTAGE) + K_SOLVER_RESIDUAL_TOLERANCE / slope(outputs[0]) +
                                 16.0 * kU * (std::fabs(source) + std::fabs(outputs[0]));
            worst = std::max(worst, TestSupport::measure_error(error, 0.0, bound).ratio);
        }
        return worst;
    }

    constexpr double K_TRACK_OHMS = 10000.0;
    constexpr double K_POT_CAPACITANCE = 100e-9;

    /// @brief Holds the DC gain a potentiometer position settles to.
    struct PotRow
    {
            const char* name;
            double position;
            double gain;
    };

    // The gain is the position; the end stops floor the shorted section at K_MINIMUM_POT_OHMS.
    const PotRow kPotRows[] = {
        {"QuarterTravel", 0.25, 0.25},
        {"HalfTravel", 0.5, 0.5},
        {"NinetyPercent", 0.9, 0.9},
        {"BottomStop", 0.0, SushiDSP::dk::K_MINIMUM_POT_OHMS / (K_TRACK_OHMS + SushiDSP::dk::K_MINIMUM_POT_OHMS)},
        {"TopStop", 1.0, K_TRACK_OHMS / (K_TRACK_OHMS + SushiDSP::dk::K_MINIMUM_POT_OHMS)},
    };

    /// @brief Builds the potentiometer divider into a capacitor and returns the index of the pot.
    std::size_t build_pot_divider(LowpassNetlist& netlist)
    {
        netlist.node_count = 2;
        netlist.voltage_source(1, 0);
        const std::size_t volume = netlist.potentiometer(0, 2, 1, K_TRACK_OHMS);
        netlist.capacitor(2, 0, K_POT_CAPACITANCE);
        netlist.output(2, 0);
        return volume;
    }

    /// @brief Returns the output of the divider after 1 V has been applied long enough to settle.
    /// tau is at most 250 us, so 40000 samples settle it. See tests/foundation/math/dk/test_dk_circuit/README.md.
    double settled_gain(double position, bool& prepared)
    {
        constexpr int K_SETTLE_SAMPLES = 40000;
        LowpassNetlist netlist;
        const std::size_t volume = build_pot_divider(netlist);
        LowpassCircuit circuit;
        prepared = circuit.prepare(netlist, K_SAMPLE_RATE);
        circuit.set_potentiometer(volume, position);
        circuit.update_matrices_if_dirty();
        circuit.reset();

        SushiDSP::dk::LinearPort nonlinear;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < K_SETTLE_SAMPLES; ++sample)
        {
            circuit.process_sample(nonlinear, std::array<double, 1>{1.0}, outputs);
        }
        return outputs[0];
    }

    constexpr int K_POISON_RUN_SAMPLES = 256;
    constexpr int K_POISONED_SAMPLE = 64;
    constexpr double K_POISON_TONE_HZ = 300.0;

    /// @brief Holds what a poisoned run and an identical healthy run did over the same input.
    struct PoisonRun
    {
            bool prepared = false;
            bool all_finite = true;
            bool identical_before_the_poison = true;
            double deviation_after_the_fallback = 0.0;
            double deviation_at_the_end = 0.0;
            std::uint64_t fallbacks = 0;
            std::uint64_t reference_fallbacks = 0;
    };

    /// @brief Steps an RC lowpass with a linear branch across the cap twice, poisoning one sample of the first.
    PoisonRun run_poisoned()
    {
        auto build = [](SushiDSP::dk::DkCircuit<1, 1, 1, 1>& circuit)
        {
            SushiDSP::dk::DkNetlist<1, 1, 1, 1> netlist;
            netlist.node_count = 2;
            netlist.voltage_source(1, 0);
            netlist.resistor(1, 2, 1000.0);
            netlist.capacitor(2, 0, 100e-9);
            netlist.nonlinear(2, 0);
            netlist.output(2, 0);
            return circuit.prepare(netlist, K_SAMPLE_RATE);
        };

        PoisonRun run;
        SushiDSP::dk::DkCircuit<1, 1, 1, 1> circuit;
        SushiDSP::dk::DkCircuit<1, 1, 1, 1> reference;
        run.prepared = build(circuit) && build(reference);
        if (!run.prepared)
        {
            return run;
        }
        SushiDSP::dk::PoisonablePort port{K_POISONABLE_BRANCH_SIEMENS};
        SushiDSP::dk::PoisonablePort healthy{K_POISONABLE_BRANCH_SIEMENS};

        for (int sample = 0; sample < K_POISON_RUN_SAMPLES; ++sample)
        {
            const std::array<double, 1> inputs{std::sin(2.0 * K_PI * K_POISON_TONE_HZ * sample / K_SAMPLE_RATE)};
            std::array<double, 1> outputs{};
            std::array<double, 1> reference_outputs{};
            port.poisoned = sample == K_POISONED_SAMPLE;
            circuit.process_sample(port, inputs, outputs);
            reference.process_sample(healthy, inputs, reference_outputs);

            run.all_finite = run.all_finite && std::isfinite(outputs[0]);
            const double deviation = std::fabs(outputs[0] - reference_outputs[0]);
            if (sample < K_POISONED_SAMPLE && outputs[0] != reference_outputs[0])
            {
                run.identical_before_the_poison = false;
            }
            if (sample == K_POISONED_SAMPLE + 1)
            {
                run.deviation_after_the_fallback = deviation;
            }
            if (sample == K_POISON_RUN_SAMPLES - 1)
            {
                run.deviation_at_the_end = deviation;
            }
        }
        run.fallbacks = circuit.nan_fallback_samples();
        run.reference_fallbacks = reference.nan_fallback_samples();
        return run;
    }

    std::string bilinear_name(const ::testing::TestParamInfo<BilinearRow>& info)
    {
        return std::string(info.param.name);
    }

    std::string pot_name(const ::testing::TestParamInfo<PotRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

TEST(Small_DkCircuit, ClipperShapeStepsToAFiniteOutputWithoutFallbacks)
{
    SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
    build_clipper_shape(netlist);
    SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    SushiDSP::dk::ConductancePort<1> port{K_LINEAR_BRANCH_SIEMENS};
    std::array<double, 1> outputs{};
    circuit.process_sample(port, std::array<double, 1>{0.5}, outputs);

    EXPECT_TRUE(std::isfinite(outputs[0]));
    EXPECT_TRUE(std::isfinite(circuit.nonlinear_currents()[0]));
    EXPECT_TRUE(std::isfinite(circuit.nonlinear_voltages()[0]));
    EXPECT_EQ(circuit.total_samples(), 1u);
    EXPECT_EQ(circuit.non_converged_samples(), 0u);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
    EXPECT_LE(circuit.maximum_iterations_seen(), K_DEFAULT_ITERATION_BUDGET);
}

TEST(Small_DkCircuit, ClipperShapeHasNoReactiveStates)
{
    SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
    build_clipper_shape(netlist);
    SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));
    EXPECT_EQ(circuit.states().size(), 0u);
}

TEST(Small_DkCircuit, ResetStatisticsClearsTheSampleCount)
{
    SushiDSP::dk::DkNetlist<0, 1, 1, 1> netlist;
    build_clipper_shape(netlist);
    SushiDSP::dk::DkCircuit<0, 1, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    SushiDSP::dk::ConductancePort<1> port{K_LINEAR_BRANCH_SIEMENS};
    std::array<double, 1> outputs{};
    circuit.process_sample(port, std::array<double, 1>{0.5}, outputs);
    circuit.reset_statistics();
    EXPECT_EQ(circuit.total_samples(), 0u);
}

TEST(Small_DkCircuit, CascadeShapeStepsToAFiniteOutputWithoutFallbacks)
{
    SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
    build_cascade_shape(netlist);
    SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    SushiDSP::dk::LinearPort port;
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 8; ++sample)
    {
        circuit.process_sample(port, std::array<double, 1>{1.0}, outputs);
    }

    EXPECT_TRUE(std::isfinite(outputs[0]));
    EXPECT_TRUE(std::isfinite(circuit.states()[0]));
    EXPECT_EQ(circuit.total_samples(), 8u);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Small_DkCircuit, CascadeShapeHasNoNonlinearBranches)
{
    SushiDSP::dk::DkNetlist<2, 0, 1, 1> netlist;
    build_cascade_shape(netlist);
    SushiDSP::dk::DkCircuit<2, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));
    EXPECT_EQ(circuit.nonlinear_currents().size(), 0u);
    EXPECT_EQ(circuit.nonlinear_voltages().size(), 0u);
}

TEST(Small_DkCircuit, WidestShapeStepsToAFiniteOutputWithoutFallbacks)
{
    SushiDSP::dk::DkNetlist<4, 2, 2, 1> netlist;
    const std::size_t volume = build_widest_shape(netlist);
    SushiDSP::dk::DkCircuit<4, 2, 2, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));
    circuit.set_potentiometer(volume, 1.0);
    circuit.update_matrices_if_dirty();

    SushiDSP::dk::ConductancePort<2> port{K_LINEAR_BRANCH_SIEMENS};
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 8; ++sample)
    {
        circuit.process_sample(port, std::array<double, 2>{0.1, 250.0}, outputs);
    }

    EXPECT_TRUE(std::isfinite(outputs[0]));
    EXPECT_EQ(circuit.total_samples(), 8u);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
    EXPECT_EQ(circuit.non_converged_samples(), 0u);
}

TEST(Small_DkCircuit, WidestShapeKeepsEveryStateFinite)
{
    SushiDSP::dk::DkNetlist<4, 2, 2, 1> netlist;
    build_widest_shape(netlist);
    SushiDSP::dk::DkCircuit<4, 2, 2, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));

    SushiDSP::dk::ConductancePort<2> port{K_LINEAR_BRANCH_SIEMENS};
    std::array<double, 1> outputs{};
    for (int sample = 0; sample < 8; ++sample)
    {
        circuit.process_sample(port, std::array<double, 2>{0.1, 250.0}, outputs);
    }
    bool finite = true;
    for (std::size_t index = 0; index < 4; ++index)
    {
        finite = finite && std::isfinite(circuit.states()[index]);
    }
    EXPECT_TRUE(finite);
}

TEST(Small_DkCircuit, WidestShapeMarksItsMatricesDirtyOnlyWhenAPotentiometerMoves)
{
    SushiDSP::dk::DkNetlist<4, 2, 2, 1> netlist;
    const std::size_t volume = build_widest_shape(netlist);
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
}

class Small_DkCircuitBilinear : public ::testing::TestWithParam<BilinearRow>
{
};

// Trapezoidal DK reproduces the bilinear transform. See tests/foundation/math/dk/test_dk_circuit/README.md.
TEST_P(Small_DkCircuitBilinear, MatchesItsDifferenceEquationSampleForSample)
{
    const BilinearRun run = run_bilinear(GetParam());
    EXPECT_TRUE(run.worst.is_within()) << run.worst.describe();
}

TEST_P(Small_DkCircuitBilinear, StaysOnTheLinearPathWithoutFallbacks)
{
    const BilinearRun run = run_bilinear(GetParam());
    EXPECT_EQ(run.fallbacks, 0u);
    EXPECT_EQ(run.non_converged, 0u);
}

INSTANTIATE_TEST_SUITE_P(Lowpasses, Small_DkCircuitBilinear, ::testing::ValuesIn(kBilinearRows), bilinear_name);

// The magnitude peak sits at w0*sqrt(1 - 2*zeta^2). See tests/foundation/math/dk/test_dk_circuit/README.md.
TEST(Small_DkCircuit, SeriesRlcHasUnityGainAtDc)
{
    const SeriesRlc rlc = measure_series_rlc();
    ASSERT_EQ(rlc.impulse.size(), SeriesRlc::K_IMPULSE_LENGTH);
    EXPECT_EQ(rlc.fallbacks, 0u);

    // No current flows at DC, so the gain across C is exactly 1; the sum of n samples carries n u sum |h|.
    double magnitude = 0.0;
    for (const double sample : rlc.impulse)
    {
        magnitude += std::fabs(sample);
    }
    const double dc_gain = magnitude_at(rlc.impulse, 0.0, SeriesRlc::K_RLC_SAMPLE_RATE);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_sum_error<double>(dc_gain, 1.0, magnitude, rlc.impulse.size())));
}

TEST(Small_DkCircuit, SeriesRlcPeaksAtTheWarpedAnalogueResonance)
{
    const SeriesRlc rlc = measure_series_rlc();
    ASSERT_EQ(rlc.impulse.size(), SeriesRlc::K_IMPULSE_LENGTH);
    const double measured = peak_frequency(rlc.impulse, SeriesRlc::K_RLC_SAMPLE_RATE, 4000.0, 6000.0);
    // The grid has a step of K_PEAK_SCAN_STEP_HZ, so the nearest grid point is within that of the true peak.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(measured, SeriesRlc::expected_peak_hz(), K_PEAK_SCAN_STEP_HZ, 1.0)));
}

TEST(Small_DkCircuit, SeriesRlcPeakHasTheAnalogueMagnitude)
{
    const SeriesRlc rlc = measure_series_rlc();
    ASSERT_EQ(rlc.impulse.size(), SeriesRlc::K_IMPULSE_LENGTH);
    const double measured_hz = peak_frequency(rlc.impulse, SeriesRlc::K_RLC_SAMPLE_RATE, 4000.0, 6000.0);
    const double peak = magnitude_at(rlc.impulse, measured_hz, SeriesRlc::K_RLC_SAMPLE_RATE);
    const double expected = SeriesRlc::expected_peak_magnitude();

    // Bound: n u sum |h| for the sum, plus the peak times (half step / (2 zeta f))^2 for the grid.
    double magnitude = 0.0;
    for (const double sample : rlc.impulse)
    {
        magnitude += std::fabs(sample);
    }
    const double half_step = 0.5 * K_PEAK_SCAN_STEP_HZ;
    const double width = 2.0 * SeriesRlc::zeta() * SeriesRlc::expected_peak_hz();
    const double bound = static_cast<double>(rlc.impulse.size()) * kU * magnitude +
                         expected * (half_step / width) * (half_step / width);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(peak, expected, bound)));
}

TEST(Small_DkCircuit, DiodeClipperReachesTheTrueRootOfTheDiodeEquation)
{
    // The oracle is the bisected root of the diode equation. See tests/foundation/math/dk/test_dk_circuit/README.md.
    bool prepared = false;
    const double worst = worst_clipper_error_ratio(prepared);
    ASSERT_TRUE(prepared);
    EXPECT_LE(worst, TestSupport::K_DEFAULT_THRESHOLD);
}

class Small_DkCircuitPotentiometer : public ::testing::TestWithParam<PotRow>
{
};

TEST_P(Small_DkCircuitPotentiometer, SettlesToTheDividerRatioOfItsPosition)
{
    // The DC gain from source to wiper is the position. See tests/foundation/math/dk/test_dk_circuit/README.md.
    bool prepared = false;
    const double gain = settled_gain(GetParam().position, prepared);
    ASSERT_TRUE(prepared);

    // A pole of at most 0.92 amplifies one update's rounding by 12.5; an update is 8 operations on a unit input.
    const double bound = 8.0 * 12.5 * kU;
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(gain, GetParam().gain, bound)));
}

INSTANTIATE_TEST_SUITE_P(Positions, Small_DkCircuitPotentiometer, ::testing::ValuesIn(kPotRows), pot_name);

TEST(Small_DkCircuit, PotentiometerRaisesTheDirtyFlagOnlyWhenItsPositionChanges)
{
    LowpassNetlist netlist;
    const std::size_t volume = build_pot_divider(netlist);
    LowpassCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, K_SAMPLE_RATE));
    circuit.set_potentiometer(volume, 1.0);
    circuit.update_matrices_if_dirty();

    circuit.set_potentiometer(volume, 0.5);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());  // moved off the 1.0 end stop
    EXPECT_FALSE(circuit.update_matrices_if_dirty()); // nothing moved
    circuit.set_potentiometer(volume, 0.5);
    EXPECT_FALSE(circuit.update_matrices_if_dirty()); // same value, still not dirty
    circuit.set_potentiometer(volume, 0.9);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());
}

// One inf sample must not poison the capacitor's state. See tests/foundation/math/dk/test_dk_circuit/README.md.
TEST(Small_DkCircuit, ANonFiniteModelSampleNeverReachesTheOutput)
{
    const PoisonRun run = run_poisoned();
    ASSERT_TRUE(run.prepared);
    EXPECT_TRUE(run.all_finite);
}

TEST(Small_DkCircuit, ANonFiniteModelSampleIsCountedAsOneFallback)
{
    const PoisonRun run = run_poisoned();
    ASSERT_TRUE(run.prepared);
    EXPECT_EQ(run.fallbacks, 1u);
    EXPECT_EQ(run.reference_fallbacks, 0u);
}

TEST(Small_DkCircuit, ACircuitMatchesAHealthyOneBitForBitBeforeTheModelGoesNonFinite)
{
    const PoisonRun run = run_poisoned();
    ASSERT_TRUE(run.prepared);
    EXPECT_TRUE(run.identical_before_the_poison);
}

// The fallback costs one sample's worth of state update. See tests/foundation/math/dk/test_dk_circuit/README.md.
TEST(Small_DkCircuit, TheFallbackCostsAtMostOneSourceStepOfOutput)
{
    const PoisonRun run = run_poisoned();
    ASSERT_TRUE(run.prepared);
    // The held state misses one sample of the source, which moves by at most 2 pi f / fs per sample at unit amplitude.
    const double source_step = 2.0 * K_PI * K_POISON_TONE_HZ / K_SAMPLE_RATE;
    EXPECT_GT(run.deviation_after_the_fallback, 0.0);
    EXPECT_LE(run.deviation_after_the_fallback, source_step);
}

TEST(Small_DkCircuit, ThePoisonedCircuitReconvergesOnTheHealthyOne)
{
    const PoisonRun run = run_poisoned();
    ASSERT_TRUE(run.prepared);
    // The pole is 43/53, so 0.811^190 is below rounding; 4 roundings per update remain, through 1 / (1 - pole).
    const double pole = 43.0 / 53.0;
    const double bound = 4.0 * kU / (1.0 - pole);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(run.deviation_at_the_end, 0.0, bound)));
}

class Small_DkCircuitClamp : public ::testing::Test
{
    protected:
        static constexpr double K_STATE_LIMIT = 1000.0;
        static constexpr int K_CLAMP_SAMPLES = 64;

        /// @brief Drives the RC lowpass with an absurd source and keeps what the clamp must hold.
        void SetUp() override
        {
            LowpassNetlist netlist;
            build_rc_lowpass(netlist);
            LowpassCircuit circuit;
            prepared = circuit.prepare(netlist, K_SAMPLE_RATE);
            circuit.set_state_limit(K_STATE_LIMIT);

            SushiDSP::dk::LinearPort nonlinear;
            std::array<double, 1> outputs{};
            for (int sample = 0; sample < K_CLAMP_SAMPLES; ++sample)
            {
                circuit.process_sample(nonlinear, std::array<double, 1>{1e30}, outputs);
                finite = finite && std::isfinite(outputs[0]);
                within_limit = within_limit && std::fabs(circuit.states()[0]) <= K_STATE_LIMIT;
            }
            final_state = std::fabs(circuit.states()[0]);
            fallbacks = circuit.nan_fallback_samples();
        }

        bool prepared = false;
        bool finite = true;
        bool within_limit = true;
        double final_state = 0.0;
        std::uint64_t fallbacks = 0;
};

// The output stays finite and the state stops at a limit. See tests/foundation/math/dk/test_dk_circuit/README.md.
TEST_F(Small_DkCircuitClamp, KeepsTheOutputFiniteUnderAnAbsurdInput)
{
    ASSERT_TRUE(prepared);
    EXPECT_TRUE(finite);
    EXPECT_EQ(fallbacks, 0u);
}

TEST_F(Small_DkCircuitClamp, StopsTheStateAtItsLimit)
{
    ASSERT_TRUE(prepared);
    EXPECT_TRUE(within_limit);
    EXPECT_EQ(final_state, K_STATE_LIMIT);
}
