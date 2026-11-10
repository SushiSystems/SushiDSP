/**************************************************************************/
/* test_clip_stage.cpp                                                    */
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
 * @file test_clip_stage.cpp
 * @brief Tests for the Tube Screamer clip block on the DK core.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <SushiDSP/foundation/circuits/models/silicon_diode.hpp>
#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/resampler.hpp>
#include <SushiDSP/nodes/tubescreamer/diode_pair_port_adapters.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp>

#include <SushiDSP/foundation/circuit_blocks/dk_block_node.hpp>
#include <SushiDSP/foundation/spice_reference/spice_block_compare.hpp>
#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/spice_reference/spice_csv.hpp>

#include "support/clip_stage_reference.hpp"

namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The three DRIVE values ts_clip.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> K_STEPPED_DRIVE{0.1, 0.5, 0.9};

    /// @brief The deviation, in dB, the small-signal response may show against its closed form.
    /// Characterized: the discrete-time block at 384 kHz against the continuous closed form.
    constexpr double K_CLOSED_FORM_TOLERANCE_DB = 0.02;

    /// @brief The relative bound on the clipped peak against the LTspice reference transient.
    /// Characterized: the figure the reference was matched to.
    constexpr double K_PEAK_RELATIVE_BOUND = 0.01;

    /// @brief The share, in percent, of samples whose Newton iteration may fail to converge.
    /// Specified: docs/design/SUSHIDSP/DSP_MATH_LIBRARY.md, 0.01 %.
    constexpr double K_MAX_NON_CONVERGED_PERCENT = 0.01;

    /// @brief The same share for the antialiased port. Characterized: 0.0105 % measured against
    /// 0.0054 % for the plain port (README), so about twice the plain bound.
    constexpr double K_ADAA_MAX_NON_CONVERGED_PERCENT = 0.02;

    /// @brief The null residual, in dB, the plain port must reach against the LTspice transient.
    /// Characterized: a regression floor at the measured figure.
    constexpr double K_PLAIN_TRANSIENT_NULL_BOUND_DB = -60.0;

    /// @brief The null residual, in dB, the antialiased port must reach against the LTspice transient.
    /// Characterized: a regression floor, not an acceptance (README).
    constexpr double K_ADAA_TRANSIENT_NULL_BOUND_DB = -28.0;

    /// @brief The null residual, in dB, either port must reach against the reference decimated to
    /// the shipping run rate. Characterized: a regression floor (README).
    constexpr double K_RUN_RATE_NULL_BOUND_DB = -20.0;

    /// @brief The null residual, in dB, the plain port reached against the block's own model
    /// stepped at 1.92 MHz. Characterized: a regression floor, measured before the reference
    /// became the independent solve (README); it is the base of the bound the test derives.
    constexpr double K_CONVERGED_PLAIN_NULL_FLOOR_DB = -15.0;

    /// @brief The same floor for the antialiased port. Characterized (README).
    constexpr double K_CONVERGED_ADAA_NULL_FLOOR_DB = -4.0;

    /// @brief Solver steps per frame of the 1.92 MHz grid in the independent solve.
    constexpr std::size_t K_REFERENCE_SUBSTEPS = 8;

    /// @brief How far, relative to the signal, the converged-model floors may move when the
    /// reference is the independent solve. Derived, not measured: the block's trapezoidal rule at
    /// 1.92 MHz stands (w*h)^2 / 12 = 0.0329 from the true solution at the 192 kHz band edge, the
    /// BDF2 solve at eight substeps (w*h)^2 / 3 = 0.0021, and the triangle inequality adds them to
    /// the floor (README).
    constexpr double K_INDEPENDENT_REFERENCE_MISS_RELATIVE = 0.035;

    /// @brief The deviation, in dB, the independent solve may show against the closed form.
    /// Derived: the diode pair's cubic term leaves 3e-5 of the response at 1e-4 V and 10 kHz, BDF2
    /// 6e-6, and the bin has no leakage over whole cycles; 3.6e-5 is 3.1e-4 dB, and the bound is
    /// three times that (README).
    constexpr double K_REFERENCE_CLOSED_FORM_TOLERANCE_DB = 0.001;

    /// @brief The relative bound on the second branch being the first reversed, in the diode pair
    /// port's voltage. Characterized: the last-bit difference the solver leaves.
    constexpr double K_VOLTAGE_ASYMMETRY_BOUND = 1.0e-12;

    /// @brief The relative and absolute bounds on the antiderivative quotient against the pair
    /// current. Characterized: the central difference with a 1e-6 V step.
    constexpr double K_ANTIDERIVATIVE_RELATIVE_BOUND = 1.0e-6;
    constexpr double K_ANTIDERIVATIVE_ABSOLUTE_BOUND = 1.0e-15;

    /// @brief How far below the plain port, in dB, the antialiased port must sink the folded harmonic.
    /// Characterized (README).
    constexpr double K_ADAA_FOLD_IMPROVEMENT_DB = 15.0;

    /// @brief The level, in dB below the fundamental, the antialiased port must sink the folded
    /// harmonic to. Characterized (README).
    constexpr double K_ADAA_FOLD_FLOOR_DB = -50.0;

    /// @brief The relative bound on a port with no history stamping the endpoint current.
    /// Characterized: double rounding of the pair current.
    constexpr double K_ENDPOINT_RELATIVE_BOUND = 1.0e-12;

    /// @brief The relative shift the interval average must move the current by once there is history.
    /// Characterized (README).
    constexpr double K_HISTORY_SHIFT_RELATIVE_BOUND = 1.0e-6;

    /// @brief The closed-form small-signal response of the clip stage (D6c plan).
    ///
    /// The nullator forces V(inv) = V(in) and draws no current, so V(in) is the Cin/Rin high-pass
    /// alone; the Rg + Cg leg then draws s*Cg/(1 + s*Cg*Rg) times that out of the summing
    /// junction, and all of it arrives through the feedback admittance Yf from the output.
    /// @param frequency_hz Frequency in hertz.
    /// @param drive The drive pot's position.
    /// @return Magnitude in decibels.
    double clip_closed_form_db(double frequency_hz, double drive)
    {
        const double thermal = 1.752 * 0.025864;            // n*Vt = 0.045313728 V
        const double diode_conductance = 2.52e-9 / thermal; // g_d = 5.5612285972e-8 S
        const double feedback_ohms = K_FEEDBACK_FIXED_OHMS + drive * K_DRIVE_POT_OHMS;
        const double feedback_farads = K_FEEDBACK_FARADS + 2.0 * K_DIODE_JUNCTION_FARADS;
        const std::complex<double> s(0.0, 2.0 * K_PI * frequency_hz);
        const std::complex<double> admittance =
            1.0 / feedback_ohms + s * feedback_farads + 2.0 * diode_conductance;
        const std::complex<double> high_pass =
            (s * K_INPUT_FARADS) / (s * K_INPUT_FARADS + 1.0 / K_INPUT_LEAK_OHMS);
        const std::complex<double> response =
            high_pass *
            (1.0 + s * K_GROUND_LEG_FARADS /
                       ((1.0 + s * K_GROUND_LEG_FARADS * K_GROUND_LEG_OHMS) * admittance));
        return 20.0 * std::log10(std::abs(response));
    }

    /// @brief What the clip block did over a short run at one drive position.
    struct PositionRun
    {
            bool prepared = false;
            bool finite = true;
            std::uint64_t nan_fallbacks = 0;
    };

    /// @brief Steps the block to a drive position and runs 64 samples of 0.1 V through it.
    PositionRun run_clip_at_position(double position)
    {
        PositionRun run;
        ClipNetlist netlist;
        const std::size_t drive = build_clip_block(netlist);
        ClipCircuit circuit;
        run.prepared = circuit.prepare(netlist, 384000.0);
        circuit.set_potentiometer(drive, position);
        circuit.update_matrices_if_dirty();

        DiodePairPort port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, {0.1}, outputs);
            run.finite = run.finite && std::isfinite(outputs[0]);
        }
        run.nan_fallbacks = circuit.nan_fallback_samples();
        return run;
    }

    /// @brief Measures the small-signal gain of the plain port at one drive and frequency, in dB.
    ///
    /// Driven at 1e-4 V the diodes stay on their zero-bias conductance, the regime the closed form
    /// describes.
    double small_signal_gain_db(double drive, double frequency)
    {
        constexpr double K_RUN_RATE = 384000.0;
        constexpr double K_AMPLITUDE_VOLTS = 1.0e-4;

        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        DiodePairPort port;
        if (!circuit.prepare(netlist, K_RUN_RATE))
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        circuit.set_potentiometer(pot, drive_to_position(drive));
        circuit.update_matrices_if_dirty();

        std::array<double, 1> outputs{};
        double peak = 0.0;
        const std::size_t samples = static_cast<std::size_t>(0.5 * K_RUN_RATE);
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * K_PI * frequency * static_cast<double>(index) / K_RUN_RATE;
            circuit.process_sample(port, {K_AMPLITUDE_VOLTS * std::sin(phase)}, outputs);
            if (index > samples * 4 / 5)
            {
                peak = std::max(peak, std::abs(outputs[0]));
            }
        }
        return 20.0 * std::log10(peak / K_AMPLITUDE_VOLTS);
    }

    /// @brief Returns the settled peak, in volts, of a 0.1 V, 1 kHz sine at a stepped drive setting.
    double clipped_peak_at_setting(std::size_t setting)
    {
        constexpr double K_RUN_RATE = 192000.0;
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        DiodePairPort port;
        if (!circuit.prepare(netlist, K_RUN_RATE))
        {
            return 0.0;
        }
        circuit.set_potentiometer(pot, drive_to_position(K_STEPPED_DRIVE[setting]));
        circuit.update_matrices_if_dirty();

        std::array<double, 1> outputs{};
        double peak = 0.0;
        const std::size_t samples = static_cast<std::size_t>(0.1 * K_RUN_RATE);
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * K_PI * 1000.0 * static_cast<double>(index) / K_RUN_RATE;
            circuit.process_sample(port, {0.1 * std::sin(phase)}, outputs);
            if (index > samples / 2)
            {
                peak = std::max(peak, std::abs(outputs[0]));
            }
        }
        return peak;
    }

    /// @brief What a convergence run measured.
    struct ConvergenceRun
    {
            bool prepared = false;
            double non_converged_percent = 100.0;
            std::uint64_t nan_fallbacks = 0;
    };

    /// @brief Drives the convergence stimulus through one port and reports the miss share.
    ///
    /// Templated because the plain and the antialiased port have to be held to the same number:
    /// antialiasing leaves the Newton residual alone by construction, so a port that changed the
    /// convergence behaviour would be a port that changed the physics.
    /// @tparam Port The nonlinear port type.
    /// @return The run: the percentage of samples whose Newton iteration did not converge.
    template <typename Port> ConvergenceRun measure_clip_convergence()
    {
        constexpr double K_RUN_RATE = 384000.0;

        ConvergenceRun run;
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit circuit;
        Port port;
        run.prepared = circuit.prepare(netlist, K_RUN_RATE);
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        std::array<double, 1> outputs{};

        const double sweep_seconds = 2.0;
        const double sweep_low = 20.0;
        const double sweep_high = 20000.0;
        const double sweep_amplitude =
            0.25118864 * static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS);
        const std::size_t sweep_samples = static_cast<std::size_t>(sweep_seconds * K_RUN_RATE);
        const double rate = std::log(sweep_high / sweep_low);
        for (std::size_t index = 0; index < sweep_samples; ++index)
        {
            const double seconds = static_cast<double>(index) / K_RUN_RATE;
            const double phase = 2.0 * K_PI * sweep_low * sweep_seconds / rate *
                                 (std::exp(seconds * rate / sweep_seconds) - 1.0);
            circuit.process_sample(port, {sweep_amplitude * std::sin(phase)}, outputs);
        }

        const std::array<double, 3> burst_volts{0.05, 0.5, 5.0};
        const std::size_t burst_samples = static_cast<std::size_t>(0.25 * K_RUN_RATE);
        for (double amplitude : burst_volts)
        {
            for (std::size_t index = 0; index < burst_samples; ++index)
            {
                const double seconds = static_cast<double>(index) / K_RUN_RATE;
                const double drive = amplitude * std::sin(2.0 * K_PI * 1000.0 * seconds);
                circuit.process_sample(port, {drive}, outputs);
            }
        }

        run.non_converged_percent = 100.0 * static_cast<double>(circuit.non_converged_samples()) /
                                    static_cast<double>(circuit.total_samples());
        run.nan_fallbacks = circuit.nan_fallback_samples();
        return run;
    }

    /// @brief One setting's stimulus and reference output rows, cut from a stepped transient.
    struct SettingRows
    {
            std::vector<float> stimulus;
            std::vector<float> output;
    };

    /// @brief Cuts the rows of one setting out of a stepped transient reference.
    SettingRows extract_setting_rows(const SushiDSP::measure::Transient& reference, std::size_t setting)
    {
        SettingRows rows;
        for (std::size_t row = 0; row < reference.input.size(); ++row)
        {
            if (reference.setting[row] == static_cast<int>(setting))
            {
                rows.stimulus.push_back(reference.input[row]);
                rows.output.push_back(reference.output[row]);
            }
        }
        return rows;
    }

    /// @brief Nulls the clip block against ts_clip's LTspice transient through one port.
    /// @tparam Port The nonlinear port type.
    /// @param reference The stepped transient reference.
    /// @param setting The setting index.
    /// @return The better, in dB, of the residual against the reference and against the
    /// half-sample-delayed reference; 0 dB when the block does not prepare or the setting has no rows.
    /// @see tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md
    template <typename Port>
    double clip_transient_null_residual_db(const SushiDSP::measure::Transient& reference, std::size_t setting)
    {
        const SettingRows rows = extract_setting_rows(reference, setting);
        if (rows.output.empty())
        {
            return 0.0;
        }

        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        Port port;
        if (!circuit.prepare(netlist, reference.sample_rate))
        {
            return 0.0;
        }
        circuit.set_potentiometer(pot, drive_to_position(K_STEPPED_DRIVE[setting]));
        circuit.update_matrices_if_dirty();

        // One pre-roll pass matches LTspice's own start. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
        constexpr std::size_t K_PRE_ROLL_PASSES = 1;
        std::vector<float> circuit_output;
        std::array<double, 1> outputs{};
        for (std::size_t pass = 0; pass <= K_PRE_ROLL_PASSES; ++pass)
        {
            const bool final_pass = pass == K_PRE_ROLL_PASSES;
            for (std::size_t index = 0; index < rows.stimulus.size(); ++index)
            {
                circuit.process_sample(port, {static_cast<double>(rows.stimulus[index])}, outputs);
                if (final_pass)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        std::vector<float> reference_output = rows.output;
        std::vector<float> aligned_reference = rows.output;
        for (std::size_t index = aligned_reference.size(); index-- > 1;)
        {
            aligned_reference[index] = 0.5f * (aligned_reference[index] + aligned_reference[index - 1]);
        }

        band_limit(reference_output, reference.sample_rate, 10000.0);
        band_limit(aligned_reference, reference.sample_rate, 10000.0);
        band_limit(circuit_output, reference.sample_rate, 10000.0);

        return std::min(SushiDSP::measure::null_test(reference_output, circuit_output, 256),
                        SushiDSP::measure::null_test(aligned_reference, circuit_output, 256));
    }

    /// @brief Magnitude of one bin of a rectangular-windowed DFT, in signal units.
    /// @param signal The signal; its length is the transform length.
    /// @param bin The bin index.
    /// @return The single-sided magnitude at that bin.
    double magnitude_at_bin(const std::vector<double>& signal, std::size_t bin)
    {
        double real = 0.0;
        double imaginary = 0.0;
        const double omega =
            2.0 * K_PI * static_cast<double>(bin) / static_cast<double>(signal.size());
        for (std::size_t index = 0; index < signal.size(); ++index)
        {
            const double phase = omega * static_cast<double>(index);
            real += signal[index] * std::cos(phase);
            imaginary -= signal[index] * std::sin(phase);
        }
        return 2.0 * std::sqrt(real * real + imaginary * imaginary) /
               static_cast<double>(signal.size());
    }

    /// @brief Renders a bin-exact sine through the clip block at the shipping run rate.
    /// @tparam Port The nonlinear port type.
    /// @param tone_hz Stimulus frequency in hertz.
    /// @param amplitude_volts Stimulus peak in volts.
    /// @param output Receives the analysis frame; its size is the transform length.
    /// @return Whether the block prepared.
    template <typename Port>
    bool render_clip_tone(double tone_hz, double amplitude_volts, std::vector<double>& output)
    {
        constexpr double K_RUN_RATE = 192000.0;

        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        Port port;
        if (!circuit.prepare(netlist, K_RUN_RATE))
        {
            return false;
        }
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();

        const double omega = 2.0 * K_PI * tone_hz / K_RUN_RATE;
        const std::size_t settle = 65536;
        std::array<double, 1> solved{};
        for (std::size_t index = 0; index < settle; ++index)
        {
            const double ramp = static_cast<double>(index) / static_cast<double>(settle);
            circuit.process_sample(
                port, {amplitude_volts * ramp * std::sin(omega * static_cast<double>(index))},
                solved);
        }
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            circuit.process_sample(
                port, {amplitude_volts * std::sin(omega * static_cast<double>(index + settle))},
                solved);
            output[index] = solved[0];
        }
        return true;
    }

    /// @brief Returns how far below its fundamental the folded harmonic sits in a rendered tone, in dB.
    double folded_harmonic_db(const std::vector<double>& tone)
    {
        constexpr std::size_t K_FUNDAMENTAL_BIN = 1280; // 15 kHz
        constexpr std::size_t K_FOLD_BIN = 256;         // 3 kHz
        return 20.0 * std::log10(magnitude_at_bin(tone, K_FOLD_BIN) / magnitude_at_bin(tone, K_FUNDAMENTAL_BIN));
    }

    /// @brief Runs the recording port through a 1 kHz, 0.5 V sine at the shipping run rate.
    RecordingDiodePairPort run_recording_port()
    {
        constexpr double K_RUN_RATE = 192000.0;

        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        RecordingDiodePairPort port;
        static_cast<void>(circuit.prepare(netlist, K_RUN_RATE));
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();

        std::array<double, 1> solved{};
        for (std::size_t index = 0; index < 4096; ++index)
        {
            const double seconds = static_cast<double>(index) / K_RUN_RATE;
            circuit.process_sample(port, {0.5 * std::sin(2.0 * K_PI * 1000.0 * seconds)}, solved);
        }
        return port;
    }

    /// @brief Returns the port impedance the recording port sees at a 0.5 V input.
    double probe_port_impedance()
    {
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        RecordingDiodePairPort probe;
        static_cast<void>(circuit.prepare(netlist, 192000.0));
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();
        std::array<double, 1> solved{};
        circuit.process_sample(probe, {0.5}, solved);
        return probe.last_impedance;
    }

    /// @brief Returns the worst miss of the antiderivative quotient against the pair current,
    /// over -1.2 V to 1.2 V in 0.01 V steps, as a multiple of the allowed miss.
    ///
    /// d/dp of H(v) - F00*h(v)^2/2 comes back as h(v), checked by a central difference.
    double worst_antiderivative_miss(double impedance)
    {
        const SushiDSP::SiliconDiode diode;
        auto pair_current = [&](double v) { return diode.current(v) - diode.current(-v); };
        auto open_circuit = [&](double v) { return v - impedance * pair_current(v); };
        auto antiderivative = [&](double v)
        {
            const double current = pair_current(v);
            return diode.current_antiderivative(v) + diode.current_antiderivative(-v) - 0.5 * impedance * current * current;
        };

        constexpr double K_DELTA = 1.0e-6;
        double worst = 0.0;
        for (int step = 0; step <= 240; ++step)
        {
            const double voltage = -1.2 + 0.01 * static_cast<double>(step);
            const double divided = (antiderivative(voltage + K_DELTA) - antiderivative(voltage - K_DELTA)) /
                                   (open_circuit(voltage + K_DELTA) - open_circuit(voltage - K_DELTA));
            const double expected = pair_current(voltage);
            const double allowed = K_ANTIDERIVATIVE_RELATIVE_BOUND * std::fabs(expected) + K_ANTIDERIVATIVE_ABSOLUTE_BOUND;
            worst = std::max(worst, std::fabs(divided - expected) / allowed);
        }
        return worst;
    }

    /// @brief What the stiff port left behind after each failed solve.
    struct FailedSolveTrail
    {
            bool prepared = false;
            std::uint64_t failures = 0;
            std::uint64_t discriminating = 0;
            std::vector<std::array<double, 2>> currents;
            std::vector<std::array<double, 2>> oracle_currents;
    };

    /// @brief Drives a flipping 2 V input through the stiff port and records the currents after each
    /// failed solve beside what the diode gives at the voltages the circuit reports.
    ///
    /// A stiff diode and a flipping input exhaust the iteration budget. See
    /// tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    FailedSolveTrail record_failed_solves()
    {
        constexpr double K_RUN_RATE = 48000.0;
        constexpr std::size_t K_SAMPLES = 4096;

        FailedSolveTrail trail;
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit circuit;
        StiffDiodePairPort port;
        trail.prepared = circuit.prepare(netlist, K_RUN_RATE);
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        const SushiDSP::SiliconDiode oracle{2.52e-9, 1.0, 5.0e-3};
        std::array<double, 1> outputs{};
        for (std::size_t index = 0; index < K_SAMPLES; ++index)
        {
            const double drive = index % 2 == 0 ? 2.0 : -2.0;
            circuit.process_sample(port, {drive}, outputs);
            if (circuit.non_converged_samples() != trail.failures)
            {
                trail.failures = circuit.non_converged_samples();
                trail.discriminating += port.previous_voltage != port.last_voltage ? 1u : 0u;
                trail.currents.push_back({circuit.nonlinear_currents()[0], circuit.nonlinear_currents()[1]});
                trail.oracle_currents.push_back({oracle.current(circuit.nonlinear_voltages()[0]), oracle.current(circuit.nonlinear_voltages()[1])});
            }
        }
        return trail;
    }

    /// @brief Fails for every failed solve whose currents differ from the diode's at its voltages.
    void expect_currents_in_step_with_voltages(const FailedSolveTrail& trail)
    {
        for (std::size_t solve = 0; solve < trail.currents.size(); ++solve)
        {
            for (std::size_t branch = 0; branch < 2; ++branch)
            {
                EXPECT_DOUBLE_EQ(trail.currents[solve][branch], trail.oracle_currents[solve][branch])
                    << "failed solve " << solve << " branch " << branch;
            }
        }
    }

    /// @brief The shipping run rate of the D6c chain: 48 kHz at the 4x oversampling factor.
    constexpr double K_SHIPPING_RUN_RATE = 384000.0;

    /// @brief Samples dropped from the head of each decimated signal before nulling.
    ///
    /// The resampler's kernel reaches about 34 output samples either side, so its first outputs
    /// read a history that is still zero. 256 samples at 192 kHz is 1.3 cycles of the 1 kHz
    /// stimulus and seven times that reach, and it is dropped from the reference and the circuit
    /// output alike, so it cannot favour either port.
    constexpr std::size_t K_DECIMATION_SETTLE = 256;

    /// @brief Converts one signal from the LTspice grid to the shipping run rate.
    /// @param input The signal on the reference grid.
    /// @param source_rate The reference grid's rate in hertz.
    /// @return The signal at K_SHIPPING_RUN_RATE.
    /// @see tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md
    std::vector<float> decimate_to_run_rate(const std::vector<float>& input, double source_rate)
    {
        SushiDSP::Resampler resampler;
        const std::uint32_t frames = static_cast<std::uint32_t>(input.size());
        resampler.prepare(source_rate, K_SHIPPING_RUN_RATE, frames,
                          K_SHIPPING_RUN_RATE / source_rate);
        resampler.set_rates(source_rate, K_SHIPPING_RUN_RATE);

        std::vector<float> output(resampler.maximum_output_frames(frames) + 1u, 0.0f);
        const std::uint32_t written = resampler.process(input.data(), frames, output.data(),
                                                        static_cast<std::uint32_t>(output.size()));
        output.resize(written);
        return output;
    }

    /// @brief Runs one port at the shipping rate against the decimated reference.
    /// @tparam Port The nonlinear port type.
    /// @param stimulus The decimated stimulus in volts.
    /// @param reference The decimated reference output, already head-trimmed.
    /// @param drive The drive pot's position.
    /// @return Null residual in dB; 0 dB when the block does not prepare.
    template <typename Port>
    double clip_run_rate_residual(const std::vector<float>& stimulus,
                                  const std::vector<float>& reference, double drive)
    {
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        Port port;
        if (!circuit.prepare(netlist, K_SHIPPING_RUN_RATE))
        {
            return 0.0;
        }
        circuit.set_potentiometer(pot, drive_to_position(drive));
        circuit.update_matrices_if_dirty();

        // The block is compared on the replay of the stimulus. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
        std::vector<float> circuit_output;
        std::array<double, 1> outputs{};
        for (std::size_t pass = 0; pass < 2; ++pass)
        {
            for (std::size_t index = 0; index < stimulus.size(); ++index)
            {
                circuit.process_sample(port, {static_cast<double>(stimulus[index])}, outputs);
                if (pass == 1 && index >= K_DECIMATION_SETTLE)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        std::vector<float> trimmed_reference = reference;
        band_limit(trimmed_reference, K_SHIPPING_RUN_RATE, 10000.0);
        band_limit(circuit_output, K_SHIPPING_RUN_RATE, 10000.0);
        return SushiDSP::measure::null_test(trimmed_reference, circuit_output, 64);
    }

    /// @brief Returns the null residual of one port against the reference decimated to the shipping
    /// run rate at one setting; 0 dB when the decimated reference is too short to trim.
    template <typename Port>
    double run_rate_null_residual_db(const SushiDSP::measure::Transient& reference, std::size_t setting)
    {
        const SettingRows rows = extract_setting_rows(reference, setting);
        const std::vector<float> decimated_stimulus = decimate_to_run_rate(rows.stimulus, reference.sample_rate);
        const std::vector<float> decimated_reference = decimate_to_run_rate(rows.output, reference.sample_rate);
        if (decimated_reference.size() <= K_DECIMATION_SETTLE * 2u)
        {
            return 0.0;
        }
        const std::vector<float> trimmed(decimated_reference.begin() + static_cast<std::ptrdiff_t>(K_DECIMATION_SETTLE),
                                         decimated_reference.end());
        return clip_run_rate_residual<Port>(decimated_stimulus, trimmed, K_STEPPED_DRIVE[setting]);
    }

    /// @brief Rate the converged reference grid uses: five times the shipping run rate.
    ///
    /// At 1.92 MHz the 13th harmonic of a 15 kHz tone sits at 195 kHz against a Nyquist of
    /// 960 kHz, so the folds that set the shipping alias floor have fallen far below the noise and
    /// the solve on this grid is a converged one, which is what a reference has to be.
    constexpr double K_CONVERGED_RUN_RATE = 1920000.0;

    /// @brief Renders a bin-exact sine through the clip block at an arbitrary rate.
    /// @tparam Port The nonlinear port type.
    /// @param run_rate Rate to step the block at, in hertz.
    /// @param tone_hz Stimulus frequency in hertz.
    /// @param amplitude_volts Stimulus peak in volts.
    /// @param frames Analysis frames to keep after settling.
    /// @return The analysis window; empty when the block does not prepare.
    template <typename Port>
    std::vector<float> render_clip_at_rate(double run_rate, double tone_hz, double amplitude_volts,
                                           std::size_t frames)
    {
        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit circuit;
        Port port;
        if (!circuit.prepare(netlist, run_rate))
        {
            return {};
        }
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();

        const double omega = 2.0 * K_PI * tone_hz / run_rate;
        const std::size_t settle = static_cast<std::size_t>(0.05 * run_rate);
        std::array<double, 1> outputs{};
        for (std::size_t index = 0; index < settle; ++index)
        {
            const double ramp = static_cast<double>(index) / static_cast<double>(settle);
            circuit.process_sample(
                port, {amplitude_volts * ramp * std::sin(omega * static_cast<double>(index))},
                outputs);
        }

        std::vector<float> window;
        window.reserve(frames);
        for (std::size_t index = 0; index < frames; ++index)
        {
            circuit.process_sample(
                port, {amplitude_volts * std::sin(omega * static_cast<double>(index + settle))},
                outputs);
            window.push_back(static_cast<float>(outputs[0]));
        }
        return window;
    }

    /// @brief Widens a null floor by the distance the independent reference may stand from the
    /// block's own converged model.
    /// @param floor_db The floor against the converged model, in dB.
    /// @return The bound against the independent solve, in dB.
    double independent_reference_bound_db(double floor_db)
    {
        return 20.0 * std::log10(std::pow(10.0, floor_db / 20.0) + K_INDEPENDENT_REFERENCE_MISS_RELATIVE);
    }

    /// @brief Solves the clip stage at the converged grid rate, with the ramped sine the block
    /// renders take, and converts the frames to the decimator's sample type.
    /// @param tone_hz Stimulus frequency in hertz.
    /// @param amplitude_volts Stimulus peak in volts.
    /// @param frames Analysis frames to keep after settling.
    /// @return The frames from the same instants render_clip_at_rate keeps.
    std::vector<float> solve_clip_at_converged_rate(double tone_hz, double amplitude_volts, std::size_t frames)
    {
        const std::size_t settle = static_cast<std::size_t>(0.05 * K_CONVERGED_RUN_RATE);
        auto stimulus = [&](double seconds)
        {
            const double ramp = std::min(1.0, seconds * K_CONVERGED_RUN_RATE / static_cast<double>(settle));
            return amplitude_volts * ramp * std::sin(2.0 * K_PI * tone_hz * seconds);
        };
        const std::vector<double> solved = TestSupport::solve_clip_stage(
            stimulus, {K_CONVERGED_RUN_RATE, K_REFERENCE_SUBSTEPS, settle, frames, drive_to_position(1.0)});
        return std::vector<float>(solved.begin(), solved.end());
    }

    /// @brief Drops the decimation settle from the head of a signal.
    std::vector<float> trim_decimation_settle(const std::vector<float>& signal)
    {
        return std::vector<float>(signal.begin() + static_cast<std::ptrdiff_t>(K_DECIMATION_SETTLE), signal.end());
    }

    /// @brief Names a drive and a frequency of the small-signal sweep.
    struct SmallSignalRow
    {
            const char* name;
            std::size_t setting;
            double frequency;
    };

    /// @brief Names a drive position.
    struct PositionRow
    {
            const char* name;
            double position;
    };

    /// @brief Names a setting index of the stepped reference and the peak the reference gives it.
    struct SettingRow
    {
            const char* name;
            std::size_t setting;
            double reference_peak_volts;
    };

    /// @brief Names a setting index of the stepped reference.
    struct ReferenceRow
    {
            const char* name;
            std::size_t setting;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

class Small_TubeScreamerCircuitsClipPosition : public ::testing::TestWithParam<PositionRow>
{
};

TEST_P(Small_TubeScreamerCircuitsClipPosition, DkTsClipBlockStaysFiniteAtTheDrivePosition)
{
    const PositionRun run = run_clip_at_position(GetParam().position);

    ASSERT_TRUE(run.prepared);
    EXPECT_TRUE(run.finite);
}

TEST_P(Small_TubeScreamerCircuitsClipPosition, DkTsClipBlockNeverFallsBackToNanAtTheDrivePosition)
{
    const PositionRun run = run_clip_at_position(GetParam().position);

    ASSERT_TRUE(run.prepared);
    EXPECT_EQ(run.nan_fallbacks, 0u);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsClipPosition,
                         ::testing::Values(PositionRow{"BottomStop", 0.0}, PositionRow{"Drive0p1", 0.1}, PositionRow{"Drive0p5", 0.5},
                                           PositionRow{"Drive0p9", 0.9}, PositionRow{"TopStop", 1.0}),
                         row_name<PositionRow>);

class Medium_TubeScreamerCircuitsClipSmallSignal : public ::testing::TestWithParam<SmallSignalRow>
{
};

TEST_P(Medium_TubeScreamerCircuitsClipSmallSignal, DkTsClipBlockSmallSignalResponseMatchesItsClosedForm)
{
    // At 1e-4 V the diodes stay on zero-bias conductance. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const SmallSignalRow& row = GetParam();
    const double drive = K_STEPPED_DRIVE[row.setting];

    EXPECT_NEAR(small_signal_gain_db(drive, row.frequency), clip_closed_form_db(row.frequency, drive), K_CLOSED_FORM_TOLERANCE_DB);
}

INSTANTIATE_TEST_SUITE_P(Rows, Medium_TubeScreamerCircuitsClipSmallSignal,
                         ::testing::Values(SmallSignalRow{"Drive0p1At100Hz", 0, 100.0}, SmallSignalRow{"Drive0p1At1000Hz", 0, 1000.0},
                                           SmallSignalRow{"Drive0p5At100Hz", 1, 100.0}, SmallSignalRow{"Drive0p5At1000Hz", 1, 1000.0},
                                           SmallSignalRow{"Drive0p9At100Hz", 2, 100.0}, SmallSignalRow{"Drive0p9At1000Hz", 2, 1000.0}),
                         row_name<SmallSignalRow>);

class Small_TubeScreamerCircuitsClipPeak : public ::testing::TestWithParam<SettingRow>
{
};

TEST_P(Small_TubeScreamerCircuitsClipPeak, DkTsClipBlockClipsAtTheReferencePeakVoltage)
{
    // The diode pair, not Rf, sets the peak above threshold. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const SettingRow& row = GetParam();

    EXPECT_NEAR(clipped_peak_at_setting(row.setting), row.reference_peak_volts, K_PEAK_RELATIVE_BOUND * row.reference_peak_volts);
}

// The peaks are the LTspice reference transient's, for 100 mV at 1 kHz (README).
INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsClipPeak,
                         ::testing::Values(SettingRow{"Drive0p1", 0, 0.481683}, SettingRow{"Drive0p5", 1, 0.490169},
                                           SettingRow{"Drive0p9", 2, 0.491758}),
                         row_name<SettingRow>);

TEST(Medium_TubeScreamerCircuits, DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // The stimulus is a sweep and three bursts at 384 kHz. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const ConvergenceRun run = measure_clip_convergence<DiodePairPort>();

    ASSERT_TRUE(run.prepared);
    EXPECT_LE(run.non_converged_percent, K_MAX_NON_CONVERGED_PERCENT);
    EXPECT_EQ(run.nan_fallbacks, 0u);
}

TEST(Medium_TubeScreamerCircuits, DkTsClipBlockAdaaPortKeepsTheNewtonMissShareUnderTwiceThePlainBound)
{
    // The ADAA port measures 0.0105 % against 0.0054 %. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const ConvergenceRun run = measure_clip_convergence<AdaaDiodePairPort>();

    ASSERT_TRUE(run.prepared);
    EXPECT_LE(run.non_converged_percent, K_ADAA_MAX_NON_CONVERGED_PERCENT);
    EXPECT_EQ(run.nan_fallbacks, 0u);
}

/// @brief Loads ts_clip's LTspice transient, and skips the test when the file is absent.
class Small_TubeScreamerCircuitsClipReference : public ::testing::TestWithParam<ReferenceRow>
{
    protected:
        void SetUp() override
        {
            const std::string path = spice_path("ts_clip", "tran");
            reference_ = SushiDSP::measure::load_transient_csv(path);
            if (reference_.input.empty())
            {
                GTEST_SKIP() << "No LTspice reference at " << path
                             << ". Generate it with: python tools/spice/export.py --block ts_clip "
                             << "(needs LTspice installed; see docs/reference/spice/README.md), "
                             << "then commit the CSV.";
            }
        }

        SushiDSP::measure::Transient reference_;
};

TEST_P(Small_TubeScreamerCircuitsClipReference, DkTsClipBlockNullsAgainstTheLtspiceTransientReference)
{
    ASSERT_GT(reference_.sample_rate, 0.0);

    EXPECT_LE(clip_transient_null_residual_db<DiodePairPort>(reference_, GetParam().setting), K_PLAIN_TRANSIENT_NULL_BOUND_DB);
}

TEST_P(Small_TubeScreamerCircuitsClipReference, DkTsClipBlockAdaaPortFallsShortOfTheLtspiceTransientReferenceByItsOwnBandLimiting)
{
    // The bound is a regression floor, not an acceptance. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    ASSERT_GT(reference_.sample_rate, 0.0);

    EXPECT_LE(clip_transient_null_residual_db<AdaaDiodePairPort>(reference_, GetParam().setting), K_ADAA_TRANSIENT_NULL_BOUND_DB);
}

TEST_P(Small_TubeScreamerCircuitsClipReference, DkTsClipBlockPlainPortNullsAgainstTheReferenceDecimatedToTheShippingRunRate)
{
    // Which port wins is not asserted. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    ASSERT_GT(reference_.sample_rate, 0.0);

    EXPECT_LE(run_rate_null_residual_db<DiodePairPort>(reference_, GetParam().setting), K_RUN_RATE_NULL_BOUND_DB);
}

TEST_P(Small_TubeScreamerCircuitsClipReference, DkTsClipBlockAdaaPortNullsAgainstTheReferenceDecimatedToTheShippingRunRate)
{
    ASSERT_GT(reference_.sample_rate, 0.0);

    EXPECT_LE(run_rate_null_residual_db<AdaaDiodePairPort>(reference_, GetParam().setting), K_RUN_RATE_NULL_BOUND_DB);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsClipReference,
                         ::testing::Values(ReferenceRow{"Drive0p1", 0}, ReferenceRow{"Drive0p5", 1}, ReferenceRow{"Drive0p9", 2}),
                         row_name<ReferenceRow>);

TEST(Small_TubeScreamerCircuits, DkTsClipBlockLeavesItsCurrentsInStepWithItsVoltagesAfterAFailedSolve)
{
    // A stiff diode and a flipping input exhaust the budget. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const FailedSolveTrail trail = record_failed_solves();

    ASSERT_TRUE(trail.prepared);
    ASSERT_GT(trail.failures, 0u) << "the stimulus never reached DkCircuit's failure path";
    ASSERT_GT(trail.discriminating, 0u) << "every failed solve returned the iterate the models were "
                                           "last evaluated at, so the assertion below is vacuous";
    expect_currents_in_step_with_voltages(trail);
}

TEST(Small_TubeScreamerCircuits, DkTsClipBlockDiodePairPortIsOneScalarPortInDisguise)
{
    // Branch 1 is branch 0 reversed, to the last bit. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const RecordingDiodePairPort port = run_recording_port();

    EXPECT_EQ(port.worst_open_circuit_asymmetry, 0.0);
    EXPECT_EQ(port.worst_impedance_asymmetry, 0.0);
    EXPECT_LT(port.worst_voltage_asymmetry, K_VOLTAGE_ASYMMETRY_BOUND);
    EXPECT_LT(port.last_impedance, 0.0);
}

TEST(Small_TubeScreamerCircuits, AdaaDiodePairPortAntiderivativeDividesBackToThePairCurrent)
{
    // d/dp of H(v) - F00*h(v)^2/2 comes back as h(v). See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const double impedance = probe_port_impedance();

    ASSERT_LT(impedance, 0.0);
    EXPECT_LE(worst_antiderivative_miss(impedance), 1.0);
}

TEST(Medium_TubeScreamerCircuits, AdaaDiodePairPortSinksTheFoldedHarmonicThatSetsTheAliasFloor)
{
    // The 13th harmonic of 15 kHz folds to 3 kHz at 192 kHz. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr std::size_t K_FRAMES = 16384;
    std::vector<double> plain(K_FRAMES, 0.0);
    std::vector<double> antialiased(K_FRAMES, 0.0);
    ASSERT_TRUE(render_clip_tone<DiodePairPort>(15000.0, 0.5, plain));
    ASSERT_TRUE(render_clip_tone<AdaaDiodePairPort>(15000.0, 0.5, antialiased));

    const double plain_db = folded_harmonic_db(plain);
    const double antialiased_db = folded_harmonic_db(antialiased);

    EXPECT_LE(antialiased_db, plain_db - K_ADAA_FOLD_IMPROVEMENT_DB);
    EXPECT_LE(antialiased_db, K_ADAA_FOLD_FLOOR_DB);
}

TEST(Small_TubeScreamerCircuits, AdaaDiodePairPortResetDropsItsSampleIntervalHistory)
{
    // A port with no history stamps the plain pair current. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const SushiDSP::SiliconDiode diode;
    AdaaDiodePairPort port;

    const std::array<double, 2> voltage{0.6, -0.6};
    const std::array<double, 2> open_circuit{1.4, -1.4};
    const std::array<std::array<double, 2>, 2> impedance{
        {{-40000.0, 40000.0}, {40000.0, -40000.0}}};
    std::array<double, 2> currents{};

    port.currents_for_update(open_circuit, voltage, impedance, currents);
    const double endpoint_at_six = diode.current(0.6) - diode.current(-0.6);
    EXPECT_NEAR(currents[0] - currents[1], endpoint_at_six, K_ENDPOINT_RELATIVE_BOUND * std::fabs(endpoint_at_six));
    EXPECT_DOUBLE_EQ(currents[1], -currents[0]);

    const std::array<double, 2> moved_voltage{0.5, -0.5};
    const std::array<double, 2> moved_open_circuit{0.9, -0.9};
    const double endpoint_at_five = diode.current(0.5) - diode.current(-0.5);

    // A second sample has an interval to average over, so it no longer sits on the endpoint.
    port.currents_for_update(moved_open_circuit, moved_voltage, impedance, currents);
    EXPECT_GT(std::fabs(currents[0] - currents[1] - endpoint_at_five),
              K_HISTORY_SHIFT_RELATIVE_BOUND * std::fabs(endpoint_at_five));

    port.reset();
    port.currents_for_update(moved_open_circuit, moved_voltage, impedance, currents);
    EXPECT_NEAR(currents[0] - currents[1], endpoint_at_five, K_ENDPOINT_RELATIVE_BOUND * std::fabs(endpoint_at_five));
}

TEST(Medium_TubeScreamerCircuits, DkTsClipBlockSmallSignalResponseOfTheIndependentSolveMatchesItsClosedForm)
{
    // The independent solve shares no code with the block. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr double K_TONE = 10000.0;
    constexpr double K_AMPLITUDE_VOLTS = 1.0e-4;
    constexpr std::size_t K_SETTLE_FRAMES = 28800; // 15 ms, 150 cycles
    constexpr std::size_t K_FRAMES = 9600;         // 50 cycles, so the bin below holds no leakage
    constexpr std::size_t K_TONE_BIN = 50;

    auto stimulus = [&](double seconds) { return K_AMPLITUDE_VOLTS * std::sin(2.0 * K_PI * K_TONE * seconds); };
    const std::vector<double> solved = TestSupport::solve_clip_stage(
        stimulus, {K_CONVERGED_RUN_RATE, K_REFERENCE_SUBSTEPS, K_SETTLE_FRAMES, K_FRAMES, 1.0});
    ASSERT_EQ(solved.size(), K_FRAMES);

    const double gain_db = 20.0 * std::log10(magnitude_at_bin(solved, K_TONE_BIN) / K_AMPLITUDE_VOLTS);

    EXPECT_NEAR(gain_db, clip_closed_form_db(K_TONE, 1.0), K_REFERENCE_CLOSED_FORM_TOLERANCE_DB);
}

TEST(Medium_TubeScreamerCircuits, DkTsClipBlockBothPortsNullAgainstAnIndependentSolveOnTheToneThatAliases)
{
    // The reference is tests/support/clip_stage_reference.hpp, not the block. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr double K_TONE = 15000.0;
    constexpr double K_AMPLITUDE_VOLTS = 0.5;
    constexpr std::size_t K_FRAMES = 16384;

    const std::vector<float> converged = solve_clip_at_converged_rate(
        K_TONE, K_AMPLITUDE_VOLTS,
        static_cast<std::size_t>(K_FRAMES * (K_CONVERGED_RUN_RATE / K_SHIPPING_RUN_RATE)));
    ASSERT_FALSE(converged.empty());
    const std::vector<float> reference = decimate_to_run_rate(converged, K_CONVERGED_RUN_RATE);
    ASSERT_GT(reference.size(), K_DECIMATION_SETTLE * 2u);

    const std::vector<float> plain = render_clip_at_rate<DiodePairPort>(K_SHIPPING_RUN_RATE, K_TONE, K_AMPLITUDE_VOLTS, K_FRAMES);
    const std::vector<float> antialiased = render_clip_at_rate<AdaaDiodePairPort>(K_SHIPPING_RUN_RATE, K_TONE, K_AMPLITUDE_VOLTS, K_FRAMES);
    ASSERT_FALSE(plain.empty());
    ASSERT_FALSE(antialiased.empty());

    // Both candidates are trimmed by the same amount. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    std::vector<float> reference_window = trim_decimation_settle(reference);
    std::vector<float> plain_window = trim_decimation_settle(plain);
    std::vector<float> antialiased_window = trim_decimation_settle(antialiased);
    const std::size_t length = std::min({reference_window.size(), plain_window.size(), antialiased_window.size()});
    reference_window.resize(length);
    plain_window.resize(length);
    antialiased_window.resize(length);

    // The floors are widened by the derived miss of the reference. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    EXPECT_LE(SushiDSP::measure::null_test(reference_window, plain_window, 64),
              independent_reference_bound_db(K_CONVERGED_PLAIN_NULL_FLOOR_DB));
    EXPECT_LE(SushiDSP::measure::null_test(reference_window, antialiased_window, 64),
              independent_reference_bound_db(K_CONVERGED_ADAA_NULL_FLOOR_DB));
}
