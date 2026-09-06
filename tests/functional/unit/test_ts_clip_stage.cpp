/**************************************************************************/
/* test_ts_clip_stage.cpp                                                 */
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

/**
 * @file test_ts_clip_stage.cpp
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
#include <cstdio>
#include <string>
#include <vector>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"
#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/tubescreamer/TubeScreamerCircuits.hpp"

#include "circuits/DkBlockNode.hpp"
#include "circuits/SpiceBlockCompare.hpp"
#include "measure/NullTest.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ClipNode = DkBlockNode<ClipNetlist, ClipCircuit, DiodePairPort>;

    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief The three DRIVE values ts_clip.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> kSteppedDrive{ 0.1, 0.5, 0.9 };

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
        const double thermal = 1.752 * 0.025864;                       // n*Vt = 0.045313728 V
        const double diode_conductance = 2.52e-9 / thermal;            // g_d = 5.5612285972e-8 S
        const double feedback_ohms = kFeedbackFixedOhms + drive * kDrivePotOhms;
        const double feedback_farads = kFeedbackFarads + 2.0 * kDiodeJunctionFarads;
        const std::complex<double> s(0.0, 2.0 * kPi * frequency_hz);
        const std::complex<double> admittance
            = 1.0 / feedback_ohms + s * feedback_farads + 2.0 * diode_conductance;
        const std::complex<double> high_pass
            = (s * kInputFarads) / (s * kInputFarads + 1.0 / kInputLeakOhms);
        const std::complex<double> response
            = high_pass * (1.0 + s * kGroundLegFarads
                                     / ((1.0 + s * kGroundLegFarads * kGroundLegOhms) * admittance));
        return 20.0 * std::log10(std::abs(response));
    }

    /// @brief A diode nine times stiffer than a 1N914, with the port's evaluation trail recorded.
    ///
    /// n*Vt = 5 mV instead of 45.3 mV is not a real part; it is what makes Newton run out of its
    /// eight-iteration budget on this circuit, which is the only way a test can reach
    /// DkCircuit's failure path. previous_voltage and last_voltage exist so the test can say
    /// whether the solver's returned iterate really was somewhere other than its last
    /// evaluation, which is what the failure-path re-evaluation is there to repair.
    struct StiffDiodePairPort
    {
        SushiDSP::SiliconDiode diode{ 2.52e-9, 1.0, 5.0e-3 };

        std::array<double, 2> last_voltage{};      ///< Port voltage of the most recent call.
        std::array<double, 2> previous_voltage{};  ///< Port voltage of the call before that.

        /// @brief Evaluates both diode currents and the diagonal Jacobian, recording the trail.
        /// @param v Branch voltages in volts.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j)
        {
            diode.current_and_jacobian(v[0], i[0], j[0][0]);
            diode.current_and_jacobian(v[1], i[1], j[1][1]);
            previous_voltage = last_voltage;
            last_voltage = v;
        }
    };
}

TEST(Unit_Dsp, DkTsClipBlockPreparesAtEveryDrivePositionIncludingBothEndStops)
{
    ClipNetlist       netlist;
    const std::size_t drive = build_clip_block(netlist);
    EXPECT_EQ(netlist.node_count, 6u);

    ClipCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 384000.0));
    for (const double position : { 0.0, 0.1, 0.5, 0.9, 1.0 })
    {
        circuit.set_potentiometer(drive, position);
        circuit.update_matrices_if_dirty();
        DiodePairPort         port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, { 0.1 }, outputs);
            ASSERT_TRUE(std::isfinite(outputs[0])) << position;
        }
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsClipBlockSmallSignalResponseMatchesItsClosedForm)
{
    // Driven far below the clipping threshold so the diodes stay on their zero-bias conductance,
    // which is the regime the closed form (and an LTspice .ac) describes. The pair's conductance
    // grows as cosh(w / nVt); holding the diode voltage w under 14 mV keeps it within 5 % of the
    // zero-bias value, and at the highest gain here (107.6) that means an input under
    // 0.0143 / 106.6 = 1.34e-4 V. 1e-4 V is used for all three settings.
    //
    // Closed-form values from the D6c plan, in dB:
    //          100 Hz    1000 Hz
    //   0.1   10.1534   25.1395
    //   0.5   18.8134   34.1273
    //   0.9   22.9649   38.2321
    constexpr double kRunRate = 384000.0;
    constexpr double kAmplitudeVolts = 1.0e-4;

    for (const double drive : kSteppedDrive)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        DiodePairPort     port;
        ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(drive));
        circuit.update_matrices_if_dirty();

        for (const double frequency : { 100.0, 1000.0 })
        {
            circuit.reset();
            std::array<double, 1> outputs{};
            double                peak = 0.0;
            const std::size_t     samples = static_cast<std::size_t>(0.5 * kRunRate);
            for (std::size_t index = 0; index < samples; ++index)
            {
                const double phase = 2.0 * kPi * frequency * static_cast<double>(index) / kRunRate;
                circuit.process_sample(port, { kAmplitudeVolts * std::sin(phase) }, outputs);
                if (index > samples * 4 / 5)
                {
                    peak = std::max(peak, std::abs(outputs[0]));
                }
            }
            const double measured_db = 20.0 * std::log10(peak / kAmplitudeVolts);
            EXPECT_NEAR(measured_db, clip_closed_form_db(frequency, drive), 0.02)
                << drive << " " << frequency;
        }
    }
}

TEST(Unit_Dsp, DkTsClipBlockClipsAtTheReferencePeakVoltage)
{
    // The reference transient drives 100 mV at 1 kHz and peaks at 0.481683 / 0.490169 / 0.491758 V
    // for DRIVE = 0.1 / 0.5 / 0.9 - almost independent of drive, because the diode pair, not the
    // feedback resistor, is what sets the output above threshold. Back of envelope at DRIVE = 0.1:
    // the peak drives 0.1 / |4700 - j3386.28| = 17.26 uA into the ground leg, and the diode-pair
    // voltage carrying that through Rf in parallel with the pair is 0.3886 V, so the output is
    // 0.1 + 0.3886 = 0.4886 V. This case is the legible failure the null test is not: a stamp sign
    // error moves this number by volts.
    constexpr double            kRunRate = 192000.0;
    const std::array<double, 3> expected_peak{ 0.481683, 0.490169, 0.491758 };

    for (std::size_t setting = 0; setting < kSteppedDrive.size(); ++setting)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        DiodePairPort     port;
        ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(kSteppedDrive[setting]));
        circuit.update_matrices_if_dirty();

        std::array<double, 1> outputs{};
        double                peak = 0.0;
        const std::size_t     samples = static_cast<std::size_t>(0.1 * kRunRate);
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double phase = 2.0 * kPi * 1000.0 * static_cast<double>(index) / kRunRate;
            circuit.process_sample(port, { 0.1 * std::sin(phase) }, outputs);
            if (index > samples / 2)
            {
                peak = std::max(peak, std::abs(outputs[0]));
            }
        }
        EXPECT_NEAR(peak, expected_peak[setting], 0.01 * expected_peak[setting]) << setting;
    }
}

TEST(Unit_Dsp, DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // Spec's Newton acceptance, on this block's own material: a 2 s sweep 20 Hz -> 20 kHz at
    // -12 dBFS in volts (0.251 * kFullScaleVolts = 0.502 V), then 1 kHz bursts at 0.05 V, 0.5 V
    // and 5 V, all at 384 kHz with the drive pot wide open, which is the hardest setting.
    //
    // The D6c plan instrumented this block at 2.7 Newton iterations per solve on average and 4
    // worst case, but it did that on a 0.5 V stimulus. The 5 V burst is harder than anything the
    // plan measured: it exhausts the eight-iteration budget on 57 of 1 056 000 samples, 0.0054 %,
    // which is inside the spec's 0.01 % but not the "4 worst case" the plan expected.
    constexpr double kRunRate = 384000.0;

    ClipNetlist       netlist;
    const std::size_t pot = build_clip_block(netlist);

    ClipCircuit   circuit;
    DiodePairPort port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, drive_to_position(1.0));
    circuit.update_matrices_if_dirty();
    circuit.reset_statistics();

    std::array<double, 1> outputs{};

    const double      sweep_seconds = 2.0;
    const double      sweep_low = 20.0;
    const double      sweep_high = 20000.0;
    const double      sweep_amplitude = 0.25118864 * static_cast<double>(SushiDSP::kFullScaleVolts);
    const std::size_t sweep_samples = static_cast<std::size_t>(sweep_seconds * kRunRate);
    const double      rate = std::log(sweep_high / sweep_low);
    for (std::size_t index = 0; index < sweep_samples; ++index)
    {
        const double seconds = static_cast<double>(index) / kRunRate;
        const double phase = 2.0 * kPi * sweep_low * sweep_seconds / rate
                             * (std::exp(seconds * rate / sweep_seconds) - 1.0);
        circuit.process_sample(port, { sweep_amplitude * std::sin(phase) }, outputs);
    }

    const std::array<double, 3> burst_volts{ 0.05, 0.5, 5.0 };
    const std::size_t           burst_samples = static_cast<std::size_t>(0.25 * kRunRate);
    for (double amplitude : burst_volts)
    {
        for (std::size_t index = 0; index < burst_samples; ++index)
        {
            const double seconds = static_cast<double>(index) / kRunRate;
            const double drive = amplitude * std::sin(2.0 * kPi * 1000.0 * seconds);
            circuit.process_sample(port, { drive }, outputs);
        }
    }

    const double ratio = 100.0 * static_cast<double>(circuit.non_converged_samples())
                         / static_cast<double>(circuit.total_samples());
    std::printf("[ measure ] ts_clip DK: %.4f %% non-converged (%llu of %llu), max %u iterations, "
                "%llu NaN fallbacks\n",
                ratio,
                static_cast<unsigned long long>(circuit.non_converged_samples()),
                static_cast<unsigned long long>(circuit.total_samples()),
                circuit.maximum_iterations_seen(),
                static_cast<unsigned long long>(circuit.nan_fallback_samples()));
    EXPECT_LE(ratio, 0.01);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsClipBlockNullsAgainstTheLtspiceTransientReference)
{
    const std::string path = spice_path("ts_clip", "tran");
    const auto        reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: python tools/spice/export.py --block ts_clip "
                     << "(needs LTspice installed; see docs/reference/spice/README.md), "
                     << "then commit the CSV.";
    }

    ASSERT_GT(reference.sample_rate, 0.0);

    for (std::size_t setting = 0; setting < kSteppedDrive.size(); ++setting)
    {
        std::vector<float> reference_output;
        std::vector<float> circuit_output;

        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit   circuit;
        DiodePairPort port;
        ASSERT_TRUE(circuit.prepare(netlist, reference.sample_rate));
        circuit.set_potentiometer(pot, drive_to_position(kSteppedDrive[setting]));
        circuit.update_matrices_if_dirty();

        // tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point,
        // runs 40 ms and saves the last 20 ms. This block sits at zero at DC - there is no supply
        // rail - so a DK block starting from zero state has LTspice's initial condition exactly,
        // and the saved window is 20 whole cycles of the 1 kHz stimulus. Replaying it ONCE
        // therefore reproduces LTspice's own first 20 ms, and the second pass is the comparison
        // window. Replaying it more converges this block to the periodic steady state while the
        // reference is still on its turn-on transient, and the null gets worse - see the pre-roll
        // table in the D6c plan. This is why jcm_v1a's eight passes are wrong here: that block has
        // a 360 V rail and a bias point.
        //
        // Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before
        // comparison, so the band-limiting cannot flatter either one; the top octave is where
        // LTspice's variable-step integrator and our fixed-step trapezoidal rule are known to
        // disagree (spec Risks). Computed while writing the D6c plan with exactly this procedure:
        // -80.3 / -73.7 / -76.0 dB.
        constexpr std::size_t kPreRollPasses = 1;

        std::vector<double> stimulus;
        for (std::size_t row = 0; row < reference.input.size(); ++row)
        {
            if (reference.setting[row] == static_cast<int>(setting))
            {
                stimulus.push_back(static_cast<double>(reference.input[row]));
                reference_output.push_back(reference.output[row]);
            }
        }

        std::array<double, 1> outputs{};
        for (std::size_t pass = 0; pass <= kPreRollPasses; ++pass)
        {
            const bool final_pass = pass == kPreRollPasses;
            for (std::size_t index = 0; index < stimulus.size(); ++index)
            {
                circuit.process_sample(port, { stimulus[index] }, outputs);
                if (final_pass)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        ASSERT_FALSE(reference_output.empty()) << "setting " << setting;

        band_limit(reference_output, reference.sample_rate, 10000.0);
        band_limit(circuit_output, reference.sample_rate, 10000.0);

        const double residual = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
        std::printf("[ measure ] ts_clip tran setting %zu: %.2f dB residual\n", setting, residual);
        EXPECT_LE(residual, -60.0) << "setting " << setting;
    }
}

TEST(Unit_Dsp, DkBlockNodeCarriesTheBlockThroughItsFloatBoundaryUnchanged)
{
    // tests/circuits/DkBlockNode.hpp is the adapter every block-level measurement runs through,
    // and Task 7's product node follows the same rule, so what it owes is exact: the buffer
    // sample times input_scale_volts is what the block solves, and the block's answer divided by
    // the same constant is what lands in the output buffer. A non-unity scale is used so a
    // dropped multiply on either side shows up.
    constexpr double        kRunRate = 192000.0;
    constexpr std::uint32_t kFrames = 256;
    constexpr double        kScaleVolts = 2.0;

    std::vector<float> input(kFrames, 0.0f);
    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        input[frame] = 0.05f
                       * static_cast<float>(
                           std::sin(2.0 * kPi * 1000.0 * static_cast<double>(frame) / kRunRate));
    }

    ClipNode node;
    build_clip_block(node.netlist);
    node.pot_positions[0] = drive_to_position(0.5);
    node.input_scale_volts = kScaleVolts;
    node.prepare({ kRunRate, kFrames, 1 });
    ASSERT_TRUE(node.prepared());

    std::vector<float>   output(kFrames, 0.0f);
    const float*         inputs[1] = { input.data() };
    float*               outputs[1] = { output.data() };
    SushiDSP::ProcessContext context{ outputs, inputs, kFrames, 0.0 };
    node.process(context);

    ClipNetlist       netlist;
    const std::size_t pot = build_clip_block(netlist);
    ClipCircuit       circuit;
    DiodePairPort     port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, drive_to_position(0.5));
    circuit.update_matrices_if_dirty();

    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        std::array<double, 1> solved{};
        circuit.process_sample(port, { static_cast<double>(input[frame]) * kScaleVolts }, solved);
        EXPECT_FLOAT_EQ(output[frame], static_cast<float>(solved[0] / kScaleVolts)) << frame;
    }
}

TEST(Unit_Dsp, DkTsClipBlockLeavesItsCurrentsInStepWithItsVoltagesAfterAFailedSolve)
{
    // Carried from the Task 3 review: DkCircuit re-evaluates the models on the failure path
    // because DampedNewtonSolver::fail() restores the lowest-residual iterate, which is generally
    // not the point the models were last evaluated at. The reviewer's recipe was a stiff port plus
    // set_maximum_iterations(1); DkCircuit exposes no setter for the solver's budget, so the
    // budget is exhausted the physical way instead - a diode nine times stiffer than a 1N914 and a
    // stimulus that flips sign every sample, which throws the warm start to the far side of the
    // root. It lives here rather than in test_dk_circuit.cpp because Task 5 does not touch that
    // file and this is the block whose port makes the failure reachable.
    constexpr double      kRunRate = 48000.0;
    constexpr std::size_t kSamples = 4096;

    ClipNetlist       netlist;
    const std::size_t pot = build_clip_block(netlist);

    ClipCircuit        circuit;
    StiffDiodePairPort port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, drive_to_position(1.0));
    circuit.update_matrices_if_dirty();
    circuit.reset_statistics();

    const SushiDSP::SiliconDiode oracle{ 2.52e-9, 1.0, 5.0e-3 };
    std::array<double, 1>        outputs{};
    std::uint64_t                seen_failures = 0;
    std::uint64_t                discriminating = 0;

    for (std::size_t index = 0; index < kSamples; ++index)
    {
        const double drive = index % 2 == 0 ? 2.0 : -2.0;
        circuit.process_sample(port, { drive }, outputs);
        if (circuit.non_converged_samples() == seen_failures)
        {
            continue;
        }
        seen_failures = circuit.non_converged_samples();
        if (port.previous_voltage != port.last_voltage)
        {
            ++discriminating;
        }
        for (std::size_t branch = 0; branch < 2; ++branch)
        {
            EXPECT_DOUBLE_EQ(circuit.nonlinear_currents()[branch],
                             oracle.current(circuit.nonlinear_voltages()[branch]))
                << "sample " << index << " branch " << branch;
        }
    }

    std::printf("[ measure ] ts_clip stiff port: %llu failed solves, %llu of them returning an "
                "iterate the models were not last evaluated at\n",
                static_cast<unsigned long long>(seen_failures),
                static_cast<unsigned long long>(discriminating));
    ASSERT_GT(seen_failures, 0u) << "the stimulus never reached DkCircuit's failure path";
}
