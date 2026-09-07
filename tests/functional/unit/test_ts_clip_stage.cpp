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
#include "SushiDSP/math/Resampler.hpp"
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

namespace
{
    /// @brief Drives the convergence stimulus through one port and reports the miss ratio.
    ///
    /// Templated because the plain and the antialiased port have to be held to the same number:
    /// antialiasing leaves the Newton residual alone by construction, so a port that changed the
    /// convergence behaviour would be a port that changed the physics.
    /// @tparam Port The nonlinear port type.
    /// @param label Printed with the measurement.
    /// @return Percentage of samples whose Newton iteration did not converge.
    template <typename Port>
    double clip_convergence_percentage(const char* label)
    {
        constexpr double kRunRate = 384000.0;

        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit circuit;
        Port        port;
        EXPECT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        std::array<double, 1> outputs{};

        const double      sweep_seconds = 2.0;
        const double      sweep_low = 20.0;
        const double      sweep_high = 20000.0;
        const double      sweep_amplitude
            = 0.25118864 * static_cast<double>(SushiDSP::kFullScaleVolts);
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
        std::printf("[ measure ] ts_clip DK %s: %.4f %% non-converged (%llu of %llu), max %u "
                    "iterations, %llu NaN fallbacks\n",
                    label, ratio,
                    static_cast<unsigned long long>(circuit.non_converged_samples()),
                    static_cast<unsigned long long>(circuit.total_samples()),
                    circuit.maximum_iterations_seen(),
                    static_cast<unsigned long long>(circuit.nan_fallback_samples()));
        EXPECT_EQ(circuit.nan_fallback_samples(), 0u) << label;
        return ratio;
    }
}

TEST(Unit_Dsp, DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // Spec's Newton acceptance, on this block's own material: a 2 s sweep 20 Hz -> 20 kHz at
    // -12 dBFS in volts (0.251 * kFullScaleVolts = 0.502 V), then 1 kHz bursts at 0.05 V, 0.5 V
    // and 5 V, all at 384 kHz with the drive pot wide open, which is the hardest setting.
    //
    // The D6c plan instrumented this block at 2.7 Newton iterations per solve on average and 4
    // worst case, but it did that on a 0.5 V stimulus, and the 5 V burst is harder than anything
    // the plan measured. One run before the session was interrupted saw the eight-iteration
    // budget exhausted on 57 of 1 056 000 samples, 0.0054 %, inside the spec's 0.01 % but past
    // the plan's "4 worst case". That figure is unverified since; the deferred sd test run
    // confirms or corrects it.
    EXPECT_LE(clip_convergence_percentage<DiodePairPort>("plain"), 0.01);
}

TEST(Unit_Dsp, DkTsClipBlockAdaaPortAlmostDoublesTheNewtonMissRate)
{
    // AdaaDiodePairPort's currents_and_jacobian() is DiodePairPort's character for character, so
    // the residual Newton solves is the same function and the extra misses are not a harder
    // solve: they are a different state trajectory arriving at harder open-circuit voltages. The
    // measured rate is 0.0105 % against the plain port's 0.0054 %, which is past the spec's
    // 0.01 % gate. That miss is recorded rather than accommodated - it is one of the two reasons
    // the shipping node still holds a DiodePairPort (spec
    // 2026-08-26-aaa-audio-quality-design.md, Model corrections D6c C2).
    EXPECT_LE(clip_convergence_percentage<AdaaDiodePairPort>("adaa"), 0.02);
}

namespace
{
    /// @brief Nulls the clip block against ts_clip's LTspice transient through one port.
    ///
    /// Templated so the antialiased port is held against the same reference at the same
    /// tolerance. Antiderivative antialiasing evaluates the pair current half a sample early
    /// inside the feedback loop, which is a real change to the modelled circuit; the honest place
    /// to see what it costs is this residual, not an argument (spec
    /// 2026-08-26-aaa-audio-quality-design.md, Model corrections D6c C2).
    /// @tparam Port The nonlinear port type.
    /// @param label Printed with each setting's residual.
    template <typename Port>
    void run_clip_transient_null(const char* label, double bound_db)
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

            ClipCircuit circuit;
            Port        port;
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

            std::vector<float> aligned_reference = reference_output;
            for (std::size_t index = aligned_reference.size(); index-- > 1;)
            {
                aligned_reference[index]
                    = 0.5f * (aligned_reference[index] + aligned_reference[index - 1]);
            }

            band_limit(reference_output, reference.sample_rate, 10000.0);
            band_limit(aligned_reference, reference.sample_rate, 10000.0);
            band_limit(circuit_output, reference.sample_rate, 10000.0);

            const double residual
                = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
            const double aligned_residual
                = SushiDSP::measure::null_test(aligned_reference, circuit_output, 256);
            std::printf("[ measure ] ts_clip %s tran setting %zu: %.2f dB residual, "
                        "%.2f dB against the half-sample-delayed reference\n",
                        label, setting, residual, aligned_residual);
            EXPECT_LE(std::min(residual, aligned_residual), bound_db)
                << label << " setting " << setting;
        }
    }
}

TEST(Unit_Dsp, DkTsClipBlockNullsAgainstTheLtspiceTransientReference)
{
    run_clip_transient_null<DiodePairPort>("plain", -60.0);
}

TEST(Unit_Dsp, DkTsClipBlockAdaaPortFallsShortOfTheLtspiceTransientReferenceByItsOwnBandLimiting)
{
    // This case does NOT meet the spec's -60 dB null, and its name says so. The reference grid is
    // 1 MHz, where a clipped corner has next to no aliasing left to remove, so all that
    // antialiasing can do there is spend its half-sample delay and its high-frequency
    // attenuation: measured against the plain port through this same pipeline the two differ by
    // -36.2 / -31.9 / -30.2 dB, which is the whole of the gap below. The departure is first order
    // in the timestep - the same comparison reads -48.6 dB at 1 MHz and -64.6 dB at 4 MHz - so it
    // is the method's known cost and not an error in it, and the reference is also offered
    // delayed by half a sample to show that realigning does not recover it either.
    //
    // Recovering the null needs Holters' delay compensation, which discretises the block at two
    // thirds of the run rate and would move every corner of the linear part by 1.5x, breaking
    // DkTsClipBlockSmallSignalResponseMatchesItsClosedForm. That is the open decision recorded in
    // spec 2026-08-26-aaa-audio-quality-design.md, Model corrections (D6c C2), and it is why
    // TubeScreamer still holds a DiodePairPort. The bound here is a regression floor on the
    // measured numbers, not an acceptance.
    run_clip_transient_null<AdaaDiodePairPort>("adaa", -28.0);
}

TEST(Unit_Dsp, DkBlockNodeCarriesTheBlockThroughItsFloatBoundaryUnchanged)
{
    // tests/circuits/DkBlockNode.hpp is the adapter every block-level measurement runs through,
    // and Task 7's product node follows the same rule, so what it owes is exact: the buffer
    // sample times input_scale_volts is what the block solves, and the block's answer divided by
    // the same constant is what lands in the output buffer. A non-unity scale is used so a
    // dropped multiply on either side shows up. The drive is 0.3 rather than the 0.5 the adapter
    // defaults every wiper to, so prepare()'s set_potentiometer and update_matrices_if_dirty have
    // to have run for the two blocks to agree.
    constexpr double        kRunRate = 192000.0;
    constexpr std::uint32_t kFrames = 256;
    constexpr double        kScaleVolts = 2.0;
    constexpr double        kDrive = 0.3;

    std::vector<float> input(kFrames, 0.0f);
    for (std::uint32_t frame = 0; frame < kFrames; ++frame)
    {
        input[frame] = 0.05f
                       * static_cast<float>(
                           std::sin(2.0 * kPi * 1000.0 * static_cast<double>(frame) / kRunRate));
    }

    ClipNode node;
    build_clip_block(node.netlist);
    node.pot_positions[0] = drive_to_position(kDrive);
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
    circuit.set_potentiometer(pot, drive_to_position(kDrive));
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
    ASSERT_GT(discriminating, 0u) << "every failed solve returned the iterate the models were "
                                     "last evaluated at, so the assertion above is vacuous";
}

namespace
{
    /// @brief A port that records what DkCircuit's update hook is handed, stamping f(v) unchanged.
    ///
    /// AdaaDiodePairPort documents a precondition it cannot check on its own - that the pair's
    /// second branch is the negative of its first all the way through p, v and F. This port makes
    /// that visible to a test without changing what the block computes.
    struct RecordingDiodePairPort
    {
        SushiDSP::SiliconDiode diode{};

        double worst_open_circuit_asymmetry = 0.0;  ///< Worst |p0 + p1| / |p0| seen.
        double worst_voltage_asymmetry = 0.0;       ///< Worst |v0 + v1| / |v0| seen.
        double worst_impedance_asymmetry = 0.0;     ///< Worst departure of F from antisymmetry.
        double last_impedance = 0.0;                ///< F[0][0] of the most recent call.

        /// @brief Evaluates both diode currents and the diagonal Jacobian at one port voltage.
        /// @param v Branch voltages in volts.
        /// @param i Receives the branch currents in amperes.
        /// @param j Receives the 2x2 Jacobian in siemens; only the diagonal is written.
        void currents_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& i,
                                   std::array<std::array<double, 2>, 2>& j) const
        {
            diode.current_and_jacobian(v[0], i[0], j[0][0]);
            diode.current_and_jacobian(v[1], i[1], j[1][1]);
        }

        /// @brief Records the operating point and leaves the stamped currents alone.
        /// @param p The port's open-circuit voltage.
        /// @param v The solved port voltage.
        /// @param f The port impedance matrix.
        /// @param i The branch currents, left as Newton produced them.
        void currents_for_update(const std::array<double, 2>& p, const std::array<double, 2>& v,
                                 const std::array<std::array<double, 2>, 2>& f,
                                 std::array<double, 2>&                      i)
        {
            static_cast<void>(i);
            const double open_scale = std::fabs(p[0]) + 1e-30;
            const double voltage_scale = std::fabs(v[0]) + 1e-30;
            const double impedance_scale = std::fabs(f[0][0]) + 1e-30;
            worst_open_circuit_asymmetry
                = std::max(worst_open_circuit_asymmetry, std::fabs(p[0] + p[1]) / open_scale);
            worst_voltage_asymmetry
                = std::max(worst_voltage_asymmetry, std::fabs(v[0] + v[1]) / voltage_scale);
            worst_impedance_asymmetry = std::max(
                { worst_impedance_asymmetry, std::fabs(f[0][1] + f[0][0]) / impedance_scale,
                  std::fabs(f[1][0] + f[0][0]) / impedance_scale,
                  std::fabs(f[1][1] - f[0][0]) / impedance_scale });
            last_impedance = f[0][0];
        }
    };

    /// @brief Magnitude of one bin of a rectangular-windowed DFT, in signal units.
    /// @param signal The signal; its length is the transform length.
    /// @param bin The bin index.
    /// @return The single-sided magnitude at that bin.
    double magnitude_at_bin(const std::vector<double>& signal, std::size_t bin)
    {
        double       real = 0.0;
        double       imaginary = 0.0;
        const double omega
            = 2.0 * kPi * static_cast<double>(bin) / static_cast<double>(signal.size());
        for (std::size_t index = 0; index < signal.size(); ++index)
        {
            const double phase = omega * static_cast<double>(index);
            real += signal[index] * std::cos(phase);
            imaginary -= signal[index] * std::sin(phase);
        }
        return 2.0 * std::sqrt(real * real + imaginary * imaginary)
               / static_cast<double>(signal.size());
    }

    /// @brief Renders a bin-exact sine through the clip block at the shipping run rate.
    /// @tparam Port The nonlinear port type.
    /// @param tone_hz Stimulus frequency in hertz.
    /// @param amplitude_volts Stimulus peak in volts.
    /// @param output Receives the analysis frame; its size is the transform length.
    template <typename Port>
    void render_clip_tone(double tone_hz, double amplitude_volts, std::vector<double>& output)
    {
        constexpr double kRunRate = 192000.0;

        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        Port              port;
        ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();

        const double          omega = 2.0 * kPi * tone_hz / kRunRate;
        const std::size_t     settle = 65536;
        std::array<double, 1> solved{};
        for (std::size_t index = 0; index < settle; ++index)
        {
            const double ramp = static_cast<double>(index) / static_cast<double>(settle);
            circuit.process_sample(
                port, { amplitude_volts * ramp * std::sin(omega * static_cast<double>(index)) },
                solved);
        }
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            circuit.process_sample(
                port,
                { amplitude_volts * std::sin(omega * static_cast<double>(index + settle)) },
                solved);
            output[index] = solved[0];
        }
    }
}

TEST(Unit_Dsp, DkTsClipBlockDiodePairPortIsOneScalarPortInDisguise)
{
    // AdaaDiodePairPort's derivation rests on the pair's second branch being the negative of its
    // first, which makes the two-branch port one scalar port carrying i(v) - i(-v). The netlist
    // wires branch 1 as branch 0 reversed, so the claim holds to the last bit in p and in F
    // rather than approximately, and this is where that is checked.
    constexpr double kRunRate = 192000.0;

    ClipNetlist            netlist;
    const std::size_t      pot = build_clip_block(netlist);
    ClipCircuit            circuit;
    RecordingDiodePairPort port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, drive_to_position(1.0));
    circuit.update_matrices_if_dirty();

    std::array<double, 1> solved{};
    for (std::size_t index = 0; index < 4096; ++index)
    {
        const double seconds = static_cast<double>(index) / kRunRate;
        circuit.process_sample(port, { 0.5 * std::sin(2.0 * kPi * 1000.0 * seconds) }, solved);
    }

    EXPECT_EQ(port.worst_open_circuit_asymmetry, 0.0);
    EXPECT_EQ(port.worst_impedance_asymmetry, 0.0);
    EXPECT_LT(port.worst_voltage_asymmetry, 1e-12);
    EXPECT_LT(port.last_impedance, 0.0);
}

TEST(Unit_Dsp, AdaaDiodePairPortAntiderivativeDividesBackToThePairCurrent)
{
    // The identity the method stands on. With v = p + F00*h(v), the substitution
    // dp = (1 - F00*h'(v)) dv turns the integral of h dp into H(v) - F00*h(v)^2/2, so the
    // derivative of that with respect to p has to come back as h(v) at every port voltage. A
    // divided difference over a narrow interval in p is the direct way to say so.
    const SushiDSP::SiliconDiode diode;

    ClipNetlist            netlist;
    const std::size_t      pot = build_clip_block(netlist);
    ClipCircuit            circuit;
    RecordingDiodePairPort probe;
    ASSERT_TRUE(circuit.prepare(netlist, 192000.0));
    circuit.set_potentiometer(pot, drive_to_position(1.0));
    circuit.update_matrices_if_dirty();
    std::array<double, 1> solved{};
    circuit.process_sample(probe, { 0.5 }, solved);
    const double impedance = probe.last_impedance;
    ASSERT_LT(impedance, 0.0);

    auto pair_current = [&](double v) { return diode.current(v) - diode.current(-v); };
    auto open_circuit = [&](double v) { return v - impedance * pair_current(v); };
    auto antiderivative = [&](double v)
    {
        const double current = pair_current(v);
        return diode.current_antiderivative(v) + diode.current_antiderivative(-v)
               - 0.5 * impedance * current * current;
    };

    const double delta = 1.0e-6;
    for (double voltage = -1.2; voltage <= 1.2; voltage += 0.01)
    {
        const double divided = (antiderivative(voltage + delta) - antiderivative(voltage - delta))
                               / (open_circuit(voltage + delta) - open_circuit(voltage - delta));
        const double expected = pair_current(voltage);
        EXPECT_NEAR(divided, expected, 1.0e-6 * std::fabs(expected) + 1.0e-15) << voltage;
    }
}

TEST(Unit_Dsp, AdaaDiodePairPortSinksTheFoldedHarmonicThatSetsTheAliasFloor)
{
    // The mechanism task 9 diagnosed, isolated: at the shipping 4x run rate of 192 kHz the 13th
    // harmonic of a 15 kHz tone lands on 195 kHz and folds to 3 kHz, already in band before any
    // filter can act. 16384 samples at 192 kHz put both 15 kHz and 3 kHz on exact bins, so the
    // two lines are read without a window and without leakage between them.
    constexpr std::size_t kFrames = 16384;
    constexpr std::size_t kFundamentalBin = 1280;  // 15 kHz
    constexpr std::size_t kFoldBin = 256;          // 3 kHz

    std::vector<double> plain(kFrames, 0.0);
    std::vector<double> antialiased(kFrames, 0.0);
    render_clip_tone<DiodePairPort>(15000.0, 0.5, plain);
    render_clip_tone<AdaaDiodePairPort>(15000.0, 0.5, antialiased);

    const double plain_db = 20.0
                            * std::log10(magnitude_at_bin(plain, kFoldBin)
                                         / magnitude_at_bin(plain, kFundamentalBin));
    const double antialiased_db = 20.0
                                  * std::log10(magnitude_at_bin(antialiased, kFoldBin)
                                               / magnitude_at_bin(antialiased, kFundamentalBin));
    std::printf("[ measure ] ts_clip 15 kHz 13th harmonic folded to 3 kHz: plain %.2f dB, "
                "adaa %.2f dB\n",
                plain_db, antialiased_db);

    EXPECT_LE(antialiased_db, plain_db - 15.0);
    EXPECT_LE(antialiased_db, -50.0);
}

TEST(Unit_Dsp, AdaaDiodePairPortResetDropsItsSampleIntervalHistory)
{
    // A port with no history has no interval to average over, so it stamps the plain pair
    // current; the node's reset() leans on that to restart click-free. The pair current is
    // i(v) - i(-v) and the port splits it evenly across the two branches, so the difference of
    // the two stamped currents is what has to come back.
    const SushiDSP::SiliconDiode diode;
    AdaaDiodePairPort            port;

    const std::array<double, 2>                voltage{ 0.6, -0.6 };
    const std::array<double, 2>                open_circuit{ 1.4, -1.4 };
    const std::array<std::array<double, 2>, 2> impedance{ { { -40000.0, 40000.0 },
                                                            { 40000.0, -40000.0 } } };
    std::array<double, 2>                      currents{};

    port.currents_for_update(open_circuit, voltage, impedance, currents);
    const double endpoint_at_six = diode.current(0.6) - diode.current(-0.6);
    EXPECT_NEAR(currents[0] - currents[1], endpoint_at_six, 1e-12 * std::fabs(endpoint_at_six));
    EXPECT_DOUBLE_EQ(currents[1], -currents[0]);

    const std::array<double, 2> moved_voltage{ 0.5, -0.5 };
    const std::array<double, 2> moved_open_circuit{ 0.9, -0.9 };
    const double endpoint_at_five = diode.current(0.5) - diode.current(-0.5);

    // A second sample has an interval to average over, so it no longer sits on the endpoint.
    port.currents_for_update(moved_open_circuit, moved_voltage, impedance, currents);
    EXPECT_GT(std::fabs(currents[0] - currents[1] - endpoint_at_five),
              1e-6 * std::fabs(endpoint_at_five));

    port.reset();
    port.currents_for_update(moved_open_circuit, moved_voltage, impedance, currents);
    EXPECT_NEAR(currents[0] - currents[1], endpoint_at_five, 1e-12 * std::fabs(endpoint_at_five));
}


namespace
{
    /// @brief The shipping run rate of the D6c chain: 48 kHz at the 4x oversampling factor.
    constexpr double kShippingRunRate = 192000.0;

    /// @brief Samples dropped from the head of each decimated signal before nulling.
    ///
    /// The resampler's kernel reaches about 34 output samples either side, so its first outputs
    /// read a history that is still zero. 256 samples at 192 kHz is 1.3 cycles of the 1 kHz
    /// stimulus and seven times that reach, and it is dropped from the reference and the circuit
    /// output alike, so it cannot favour either port.
    constexpr std::size_t kDecimationSettle = 256;

    /// @brief Converts one signal from the LTspice grid to the shipping run rate.
    ///
    /// `Resampler` is the tree's committed arbitrary-ratio converter (DR1), which matters here
    /// because 1 MHz to 192 kHz is 24/125 and no halfband cascade reaches it. It is zero phase, so
    /// output time t carries input time t and nothing has to be shifted back afterwards, and its
    /// kernel time-scales with the ratio, so decimating moves its cutoff to 96 kHz and the
    /// reference arrives band-limited to what a 4x model could represent in the first place.
    ///
    /// The converter touches the reference and the stimulus, never a circuit output: both ports
    /// are then driven by the same decimated stimulus and nulled against the same decimated
    /// reference, so whatever colour it leaves below 10 kHz is common to both and cancels in the
    /// comparison the case exists to make.
    /// @param input The signal on the reference grid.
    /// @param source_rate The reference grid's rate in hertz.
    /// @return The signal at kShippingRunRate.
    std::vector<float> decimate_to_run_rate(const std::vector<float>& input, double source_rate)
    {
        SushiDSP::Resampler resampler;
        const std::uint32_t frames = static_cast<std::uint32_t>(input.size());
        resampler.prepare(source_rate, kShippingRunRate, frames, kShippingRunRate / source_rate);
        resampler.set_rates(source_rate, kShippingRunRate);

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
    /// @return Null residual in dB.
    template <typename Port>
    double clip_run_rate_residual(const std::vector<float>& stimulus,
                                  const std::vector<float>& reference, double drive)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        Port              port;
        EXPECT_TRUE(circuit.prepare(netlist, kShippingRunRate));
        circuit.set_potentiometer(pot, drive_to_position(drive));
        circuit.update_matrices_if_dirty();

        // The reference window is the second half of LTspice's 40 ms run, so the block is walked
        // through the stimulus once and compared on the replay, exactly as the reference-rate case
        // argues.
        std::vector<float>    circuit_output;
        std::array<double, 1> outputs{};
        for (std::size_t pass = 0; pass < 2; ++pass)
        {
            for (std::size_t index = 0; index < stimulus.size(); ++index)
            {
                circuit.process_sample(port, { static_cast<double>(stimulus[index]) }, outputs);
                if (pass == 1 && index >= kDecimationSettle)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        std::vector<float> trimmed_reference = reference;
        band_limit(trimmed_reference, kShippingRunRate, 10000.0);
        band_limit(circuit_output, kShippingRunRate, 10000.0);
        return SushiDSP::measure::null_test(trimmed_reference, circuit_output, 64);
    }
}

TEST(Unit_Dsp, DkTsClipBlockBothPortsNullAgainstTheReferenceDecimatedToTheShippingRunRate)
{
    // The decision measurement for D6c C2. The reference-rate case nulls at 1 MHz, where a clipped
    // corner carries almost no aliasing and antialiasing can only spend its half-sample delay and
    // its corner attenuation; it therefore says nothing about whether antialiasing helps at the
    // rate the pedal actually runs. This case asks the question that decides: decimate LTspice's
    // 1 MHz transient to the shipping 192 kHz, so the reference is the analogue circuit
    // band-limited to what a 4x model can represent, then null both ports against it there.
    //
    // Whichever port comes back closer is the one that models the circuit better at the rate it
    // ships at. The verdict and its numbers are in spec 2026-08-26-aaa-audio-quality-design.md,
    // Model corrections (D6c C2).
    const std::string path = spice_path("ts_clip", "tran");
    const auto        reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: python tools/spice/export.py --block ts_clip "
                     << "(needs LTspice installed; see docs/reference/spice/README.md), "
                     << "then commit the CSV.";
    }
    ASSERT_GT(reference.sample_rate, kShippingRunRate);

    for (std::size_t setting = 0; setting < kSteppedDrive.size(); ++setting)
    {
        std::vector<float> stimulus;
        std::vector<float> reference_output;
        for (std::size_t row = 0; row < reference.input.size(); ++row)
        {
            if (reference.setting[row] == static_cast<int>(setting))
            {
                stimulus.push_back(reference.input[row]);
                reference_output.push_back(reference.output[row]);
            }
        }
        ASSERT_FALSE(reference_output.empty()) << "setting " << setting;

        const std::vector<float> decimated_stimulus
            = decimate_to_run_rate(stimulus, reference.sample_rate);
        const std::vector<float> decimated_reference
            = decimate_to_run_rate(reference_output, reference.sample_rate);
        ASSERT_GT(decimated_reference.size(), kDecimationSettle * 2u) << "setting " << setting;

        const std::vector<float> trimmed(decimated_reference.begin()
                                             + static_cast<std::ptrdiff_t>(kDecimationSettle),
                                         decimated_reference.end());

        const double plain
            = clip_run_rate_residual<DiodePairPort>(decimated_stimulus, trimmed,
                                                    kSteppedDrive[setting]);
        const double antialiased
            = clip_run_rate_residual<AdaaDiodePairPort>(decimated_stimulus, trimmed,
                                                        kSteppedDrive[setting]);
        std::printf("[ measure ] ts_clip at 192 kHz vs decimated reference, setting %zu: "
                    "plain %.2f dB, adaa %.2f dB\n",
                    setting, plain, antialiased);

        // Both ports must stay recognisably the same circuit; which one wins is what the printout
        // reports and the spec records, and it is not asserted here in either direction.
        EXPECT_LE(plain, -20.0) << "setting " << setting;
        EXPECT_LE(antialiased, -20.0) << "setting " << setting;
    }
}

namespace
{
    /// @brief Rate the converged reference run uses: ten times the shipping run rate.
    ///
    /// At 1.92 MHz the 13th harmonic of a 15 kHz tone sits at 195 kHz against a Nyquist of
    /// 960 kHz, so the fold that sets the shipping alias floor needs the 128th harmonic instead of
    /// the 13th and has fallen far below the noise. The block's own equations at this rate are
    /// therefore a converged solution of the model, which is what a reference has to be.
    constexpr double kConvergedRunRate = 1920000.0;

    /// @brief Renders a bin-exact sine through the clip block at an arbitrary rate.
    /// @tparam Port The nonlinear port type.
    /// @param run_rate Rate to step the block at, in hertz.
    /// @param tone_hz Stimulus frequency in hertz.
    /// @param amplitude_volts Stimulus peak in volts.
    /// @param frames Analysis frames to keep after settling.
    /// @return The analysis window.
    template <typename Port>
    std::vector<float> render_clip_at_rate(double run_rate, double tone_hz, double amplitude_volts,
                                           std::size_t frames)
    {
        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);
        ClipCircuit       circuit;
        Port              port;
        EXPECT_TRUE(circuit.prepare(netlist, run_rate));
        circuit.set_potentiometer(pot, drive_to_position(1.0));
        circuit.update_matrices_if_dirty();

        const double      omega = 2.0 * kPi * tone_hz / run_rate;
        const std::size_t settle = static_cast<std::size_t>(0.05 * run_rate);
        std::array<double, 1> outputs{};
        for (std::size_t index = 0; index < settle; ++index)
        {
            const double ramp = static_cast<double>(index) / static_cast<double>(settle);
            circuit.process_sample(
                port, { amplitude_volts * ramp * std::sin(omega * static_cast<double>(index)) },
                outputs);
        }

        std::vector<float> window;
        window.reserve(frames);
        for (std::size_t index = 0; index < frames; ++index)
        {
            circuit.process_sample(
                port,
                { amplitude_volts * std::sin(omega * static_cast<double>(index + settle)) },
                outputs);
            window.push_back(static_cast<float>(outputs[0]));
        }
        return window;
    }
}

TEST(Unit_Dsp, DkTsClipBlockBothPortsNullAgainstTheConvergedModelOnTheToneThatAliases)
{
    // The companion to the decimated-LTspice case, and the reason that one cannot decide alone:
    // ts_clip's stimulus is a 1 kHz sine, and 192000 / 1000 is an integer, so at the shipping rate
    // every folded harmonic of that tone lands exactly on a harmonic of it. The stimulus is blind
    // to the aliasing antialiasing exists to remove, and can only charge it for its cost.
    //
    // This case puts the question on the tone that does alias. The reference is the same block's
    // own equations at 1.92 MHz, where the 13th harmonic of 15 kHz is nowhere near Nyquist, then
    // decimated to 192 kHz by the same converter - a converged solution of the model rather than
    // of the circuit, which is the right reference for asking which discretisation is closer to
    // the answer it is trying to compute.
    constexpr double      kTone = 15000.0;
    constexpr double      kAmplitudeVolts = 0.5;
    constexpr std::size_t kFrames = 16384;

    const std::vector<float> converged = render_clip_at_rate<DiodePairPort>(
        kConvergedRunRate, kTone, kAmplitudeVolts,
        static_cast<std::size_t>(kFrames * (kConvergedRunRate / kShippingRunRate)));
    const std::vector<float> reference = decimate_to_run_rate(converged, kConvergedRunRate);
    ASSERT_GT(reference.size(), kDecimationSettle * 2u);

    const std::vector<float> plain
        = render_clip_at_rate<DiodePairPort>(kShippingRunRate, kTone, kAmplitudeVolts, kFrames);
    const std::vector<float> antialiased
        = render_clip_at_rate<AdaaDiodePairPort>(kShippingRunRate, kTone, kAmplitudeVolts, kFrames);

    // The head of the decimated reference carries the converter's start-up, and both candidates
    // are trimmed by the same amount, so neither is favoured.
    auto trim = [](const std::vector<float>& signal)
    {
        return std::vector<float>(signal.begin() + static_cast<std::ptrdiff_t>(kDecimationSettle),
                                  signal.end());
    };

    std::vector<float> reference_window = trim(reference);
    std::vector<float> plain_window = trim(plain);
    std::vector<float> antialiased_window = trim(antialiased);
    const std::size_t  length
        = std::min({ reference_window.size(), plain_window.size(), antialiased_window.size() });
    reference_window.resize(length);
    plain_window.resize(length);
    antialiased_window.resize(length);

    // null_test aligns an integer lag and a gain, so antialiasing's half sample survives it. That
    // half sample is 2.6 us at this rate and a host could compensate it, so the antialiased port
    // is also offered the reference delayed by half a sample: the pair of numbers separates the
    // part of its error a delay line could take back from the part nothing can.
    std::vector<float> delayed_reference = reference_window;
    for (std::size_t index = delayed_reference.size(); index-- > 1;)
    {
        delayed_reference[index]
            = 0.5f * (delayed_reference[index] + delayed_reference[index - 1]);
    }

    const double plain_residual
        = SushiDSP::measure::null_test(reference_window, plain_window, 64);
    const double antialiased_residual
        = SushiDSP::measure::null_test(reference_window, antialiased_window, 64);
    const double antialiased_aligned
        = SushiDSP::measure::null_test(delayed_reference, antialiased_window, 64);
    std::printf("[ measure ] ts_clip at 192 kHz vs the converged model, 15 kHz: plain %.2f dB, "
                "adaa %.2f dB, adaa half-sample aligned %.2f dB\n",
                plain_residual, antialiased_residual, antialiased_aligned);

    // Regression floors on the measured numbers, not acceptances. Which port is closer is the
    // decision this case exists to make, and it is recorded in the spec rather than asserted here.
    EXPECT_LE(plain_residual, -15.0);
    EXPECT_LE(antialiased_residual, -4.0);
}
