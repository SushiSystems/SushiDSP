/**************************************************************************/
/* test_dk_triode_stage.cpp                                               */
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

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/math/Biquad.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

#include "circuits/JcmV1aNetlist.hpp"
#include "measure/FrequencyResponse.hpp"
#include "measure/NullTest.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    // The netlist, its component constants and the triode glue live in
    // tests/circuits/JcmV1aNetlist.hpp: the RT-safety traps and the benchmark build the same
    // block, and one description of it is the only way the three stay the same circuit.
    using namespace SushiDSP::test_circuits;

    /// @brief The stage's DK shape: six capacitors, two triode branches, two sources, one probe.
    using StageCircuit = SushiDSP::dk::DkCircuit<6, 2, 2, 1>;
    using StageNetlist = SushiDSP::dk::DkNetlist<6, 2, 2, 1>;

    /// @brief Runs the stage with both sources held constant until the bias point settles.
    /// @param circuit The prepared circuit; its state is advanced in place.
    /// @param port The triode glue.
    /// @param sample_rate Run rate in hertz, matching the rate the circuit was prepared at.
    /// @param seconds Amount of silence to run.
    void settle(StageCircuit& circuit, TriodeStagePort& port, double sample_rate, double seconds)
    {
        const std::array<double, 2> inputs{ 0.0, kSupplyVolts };
        std::array<double, 1>       outputs{};
        const std::size_t           samples = static_cast<std::size_t>(seconds * sample_rate);
        for (std::size_t index = 0; index < samples; ++index)
        {
            circuit.process_sample(port, inputs, outputs);
        }
    }

    /// @brief One instance of the D6c/D6d product-node pattern, wrapping a DK block.
    ///
    /// This adapter is the template every DK-based product module will follow: the block itself
    /// is solved entirely in double, and float appears only at the IAudioNode boundary, where the
    /// host's normalised sample is scaled to volts by kFullScaleVolts on the way in and back by
    /// the same constant on the way out. prepare() does all the work that is not real-time safe -
    /// the derivation and the bias settling - so process() is only the per-sample solve.
    class JcmV1aStageNode final : public SushiDSP::IAudioNode
    {
    public:
        /// @brief Sets the preamp pot position; takes effect at the next prepare() or block.
        /// @param preamp Normalised preamp-pot fraction, matching the SPICE block's PREAMP.
        void set_preamp(double preamp) { preamp_ = preamp; }

        /// @brief Derives the DK matrices at the run rate and settles the bias point.
        /// @param info Sample rate, block size and channel count.
        void prepare(const SushiDSP::PrepareInfo& info) override
        {
            StageNetlist netlist;
            pot_index_ = build_jcm_v1a(netlist);
            sample_rate_ = info.sample_rate;
            prepared_ = circuit_.prepare(netlist, info.sample_rate);
            circuit_.set_potentiometer(pot_index_, preamp_to_position(preamp_));
            circuit_.update_matrices_if_dirty();
            settle(circuit_, port_, info.sample_rate, kSettleSeconds);
            circuit_.reset_statistics();
        }

        /// @brief Solves one sample per frame, casting float to volts and back.
        /// @param context The per-block buffers and frame count.
        void process(SushiDSP::ProcessContext& context) override
        {
            const float* input = context.inputs == nullptr ? nullptr : context.inputs[0];
            float*       output = context.outputs[0];
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                const double drive = input == nullptr
                                         ? 0.0
                                         : static_cast<double>(input[frame])
                                               * static_cast<double>(SushiDSP::kFullScaleVolts);
                const std::array<double, 2> inputs{ drive, kSupplyVolts };
                std::array<double, 1>       outputs{};
                circuit_.process_sample(port_, inputs, outputs);
                output[frame] = static_cast<float>(outputs[0]
                                                   / static_cast<double>(SushiDSP::kFullScaleVolts));
            }
        }

        /// @brief Reports the node's fixed channel counts.
        /// @return One input channel and one output channel.
        SushiDSP::PortLayout ports() const override { return SushiDSP::PortLayout{ 1u, 1u }; }

        /// @brief Reads a parameter; this fixture exposes none.
        /// @param id Ignored.
        /// @return Always zero.
        float get_parameter(const char* id) const override
        {
            static_cast<void>(id);
            return 0.0f;
        }

        /// @brief Writes a parameter; this fixture exposes none.
        /// @param id Ignored.
        /// @param value Ignored.
        void set_parameter(const char* id, float value) override
        {
            static_cast<void>(id);
            static_cast<void>(value);
        }

        /// @brief Clears the circuit state and re-settles the bias point.
        ///
        /// Prepare-class, not real-time safe: the 200 ms settle is hundreds of thousands of
        /// Newton solves. IAudioNode::reset() is documented control-thread-only, so that is
        /// legal, but a product node copying this template must keep it off the audio thread.
        void reset() override
        {
            circuit_.reset();
            settle(circuit_, port_, sample_rate_, kSettleSeconds);
        }

        /// @brief Whether prepare() derived a usable circuit.
        /// @return True when the derivation succeeded.
        bool prepared() const { return prepared_; }

    private:
        static constexpr double kSettleSeconds = 0.2;

        StageCircuit    circuit_{};
        TriodeStagePort port_{};
        std::size_t     pot_index_{ 0 };
        double          preamp_{ 0.5 };
        double          sample_rate_{ 0.0 };
        bool            prepared_{ false };
    };

    /// @brief Path of a generated LTspice CSV; a missing file means "not generated yet".
    /// @param block The SPICE block directory name.
    /// @param kind Either "ac" or "tran".
    /// @return The absolute path the export tool would write.
    std::string spice_path(const char* block, const char* kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }

    /// @brief Applies a 4th-order Butterworth low-pass, as two cascaded Biquad sections.
    ///
    /// The same filter object shape and the same cutoff are applied to the reference and to the
    /// circuit under test, so the band-limiting cannot flatter either signal: it only removes the
    /// top octave, where LTspice's variable-step integrator and our fixed-step trapezoidal rule
    /// are known to disagree (D6b plan, Risks).
    /// @param signal The signal to filter, modified in place.
    /// @param sample_rate Sample rate in hertz.
    /// @param cutoff_hz Corner frequency in hertz.
    void band_limit(std::vector<float>& signal, double sample_rate, double cutoff_hz)
    {
        SushiDSP::Biquad first;
        SushiDSP::Biquad second;
        first.set_low_pass(sample_rate, cutoff_hz, 0.54119610);
        second.set_low_pass(sample_rate, cutoff_hz, 1.30656296);
        for (float& sample : signal)
        {
            sample = second.process_sample(first.process_sample(sample));
        }
    }

    /// @brief Linearly interpolates one setting's magnitude curve out of a loaded AC sweep.
    /// @param sweep The parsed sweep.
    /// @param setting The stepped-control index to read.
    /// @param frequency_hz The frequency to evaluate at.
    /// @param magnitude_db Receives the interpolated magnitude when the frequency is in range.
    /// @return False when the setting has fewer than two rows or the frequency is out of range.
    bool interpolate_magnitude(const SushiDSP::measure::AcSweep& sweep,
                               int                               setting,
                               double                            frequency_hz,
                               double&                           magnitude_db)
    {
        bool        have_previous = false;
        double      previous_frequency = 0.0;
        double      previous_magnitude = 0.0;
        for (std::size_t row = 0; row < sweep.frequency_hz.size(); ++row)
        {
            if (sweep.setting[row] != setting)
            {
                continue;
            }
            const double frequency = sweep.frequency_hz[row];
            const double magnitude = sweep.magnitude_db[row];
            if (have_previous && frequency_hz >= previous_frequency && frequency_hz <= frequency)
            {
                const double span = frequency - previous_frequency;
                const double blend = span > 0.0 ? (frequency_hz - previous_frequency) / span : 0.0;
                magnitude_db = previous_magnitude + blend * (magnitude - previous_magnitude);
                return true;
            }
            previous_frequency = frequency;
            previous_magnitude = magnitude;
            have_previous = true;
        }
        return false;
    }

    /// @brief The three PREAMP values the SPICE block steps, in setting-index order.
    inline constexpr std::array<double, 3> kSteppedPreamp{ 0.1, 0.5, 0.9 };
}

// Spec D6b.4 acceptance: the operating point of the jcm_v1a stage.
//
// Hand solution (independent of DkCircuit, two-variable reduction plus bisection on Ip):
// Kirchhoff gives Vgk = -Ip*Rk and Vpk = 360 - Ip*(Rp + Rk) with Rk = 2.7k, Rp = 100k, so the
// whole bias reduces to the scalar fixed point Ip = koren(-Ip*Rk, 360 - Ip*102.7k). koren(...)
// - Ip is strictly decreasing over 0 to 3.4 mA (it falls from 11.3 mA at Ip = 0 to zero before
// 1.7 mA), so the root is unique; bisection to machine precision gives
//
//   Ip = 8.555837e-4 A,  Vk = 2.3101 V,  Vpk = 272.132 V,  Vp = 274.442 V.
//
// The brief's advisory band (0.8 V < Vk < 2.2 V) was derived for a 1.5k cathode resistor; with
// the block's actual 2.7k and a 360 V supply the correct cathode voltage is 2.31 V, and the hand
// solve above says so without reference to any DkCircuit output. The assertions below therefore
// pin the hand-computed fixed point directly, which is strictly stronger than the advisory band:
// any stamp sign error moves the operating point far further than these tolerances allow.
TEST(Unit_Dsp, DkTriodeStageSettlesAtTheHandComputedBiasPoint)
{
    constexpr double kRunRate = 384000.0;

    StageNetlist      netlist;
    const std::size_t pot = build_jcm_v1a(netlist);

    StageCircuit    circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, preamp_to_position(0.5));
    circuit.update_matrices_if_dirty();

    settle(circuit, port, kRunRate, 0.2);

    const double plate_current = circuit.nonlinear_currents()[1];
    const double grid_current = circuit.nonlinear_currents()[0];
    const double cathode_volts = (plate_current + grid_current) * kCathodeOhms;
    const double plate_to_cathode = circuit.nonlinear_voltages()[1];
    const double plate_volts = plate_to_cathode + cathode_volts;

    std::printf("[ measure ] jcm_v1a DK bias: Ip = %.6e A, Vk = %.4f V, Vp = %.4f V\n",
                plate_current, cathode_volts, plate_volts);

    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
    EXPECT_NEAR(cathode_volts, 2.3101, 0.02);
    EXPECT_NEAR(plate_volts, 274.442, 0.5);
    EXPECT_NEAR(circuit.nonlinear_voltages()[0], -cathode_volts, 1.0e-4);
}

TEST(Unit_Dsp, DkTriodeStageNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // Spec D6b.4: <= 0.01 % non-converged, zero NaN, over the full stimulus set. The set is a
    // 2-second sweep 20 Hz -> 20 kHz at -12 dBFS scaled to volts by kFullScaleVolts, plus a
    // 1 kHz burst at 0.05 V, 0.5 V and 5 V (well past grid conduction), all at 384 kHz.
    constexpr double kRunRate = 384000.0;

    StageNetlist      netlist;
    const std::size_t pot = build_jcm_v1a(netlist);

    StageCircuit    circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, kRunRate));
    circuit.set_potentiometer(pot, preamp_to_position(0.5));
    circuit.update_matrices_if_dirty();

    settle(circuit, port, kRunRate, 0.2);
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
        const double phase = 2.0 * 3.14159265358979323846 * sweep_low * sweep_seconds / rate
                             * (std::exp(seconds * rate / sweep_seconds) - 1.0);
        const std::array<double, 2> inputs{ sweep_amplitude * std::sin(phase), kSupplyVolts };
        circuit.process_sample(port, inputs, outputs);
    }

    const std::array<double, 3> burst_volts{ 0.05, 0.5, 5.0 };
    const std::size_t           burst_samples = static_cast<std::size_t>(0.25 * kRunRate);
    for (double amplitude : burst_volts)
    {
        for (std::size_t index = 0; index < burst_samples; ++index)
        {
            const double seconds = static_cast<double>(index) / kRunRate;
            const double drive
                = amplitude * std::sin(2.0 * 3.14159265358979323846 * 1000.0 * seconds);
            const std::array<double, 2> inputs{ drive, kSupplyVolts };
            circuit.process_sample(port, inputs, outputs);
        }
    }

    const double ratio = 100.0 * static_cast<double>(circuit.non_converged_samples())
                       / static_cast<double>(circuit.total_samples());
    std::printf("[ measure ] jcm_v1a DK: %.4f %% non-converged (%llu of %llu), max %u iterations, "
                "%llu NaN fallbacks\n",
                ratio,
                static_cast<unsigned long long>(circuit.non_converged_samples()),
                static_cast<unsigned long long>(circuit.total_samples()),
                circuit.maximum_iterations_seen(),
                static_cast<unsigned long long>(circuit.nan_fallback_samples()));
    EXPECT_LE(ratio, 0.01);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTriodeStageNullsAgainstTheLtspiceTransientReference)
{
    const std::string path = spice_path("jcm_v1a", "tran");
    const auto        reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: python tools/spice/export.py --block jcm_v1a "
                     << "(needs LTspice installed; see docs/reference/spice/README.md), "
                     << "then commit the CSV.";
    }

    ASSERT_GT(reference.sample_rate, 0.0);

    for (std::size_t setting = 0; setting < kSteppedPreamp.size(); ++setting)
    {
        std::vector<float> reference_output;
        std::vector<float> circuit_output;

        StageNetlist      netlist;
        const std::size_t pot = build_jcm_v1a(netlist);

        StageCircuit    circuit;
        TriodeStagePort port;
        ASSERT_TRUE(circuit.prepare(netlist, reference.sample_rate));
        circuit.set_potentiometer(pot, preamp_to_position(kSteppedPreamp[setting]));
        circuit.update_matrices_if_dirty();
        settle(circuit, port, reference.sample_rate, 0.2);

        // The exporter saves one 20 ms steady-state block per setting. The output high-pass
        // formed by Cc (0.022 uF) into the ~1 MOhm pot has tau ~ 22 ms, so driving that block
        // once from the quiescent bias leaves a turn-on DC transient around -43 dB that the
        // plan's 5 ms discard nowhere near removes. The block is 20 whole cycles of the 1 kHz
        // stimulus, so it repeats seamlessly: it is run kPreRollPasses times to give over 160 ms
        // (more than seven time constants, decaying the transient past -100 dB) and only the
        // final pass is compared, which is why no leading samples are discarded from it.
        constexpr std::size_t kPreRollPasses = 8;

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
                const std::array<double, 2> inputs{ stimulus[index], kSupplyVolts };
                circuit.process_sample(port, inputs, outputs);
                if (final_pass)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        ASSERT_FALSE(reference_output.empty()) << "setting " << setting;

        band_limit(reference_output, reference.sample_rate, 10000.0);
        band_limit(circuit_output, reference.sample_rate, 10000.0);

        const double residual
            = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
        std::printf("[ measure ] jcm_v1a tran setting %zu: %.2f dB residual\n", setting, residual);
        EXPECT_LE(residual, -60.0) << "setting " << setting;
    }
}

TEST(Unit_Dsp, DkTriodeStageMatchesTheLtspiceAcSweepWithinHalfADecibel)
{
    const std::string path = spice_path("jcm_v1a", "ac");
    const auto        reference = SushiDSP::measure::load_ac_csv(path);
    if (reference.frequency_hz.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: python tools/spice/export.py --block jcm_v1a "
                     << "(needs LTspice installed; see docs/reference/spice/README.md), "
                     << "then commit the CSV.";
    }

    constexpr double kRunRate = 192000.0;

    // frequency_response drives the node at -40 dBFS, which keeps the stage small-signal - the
    // regime an LTspice .ac analysis linearises around.
    const std::vector<double> frequencies{ 20.0,   50.0,   100.0,  200.0,  500.0,
                                           1000.0, 2000.0, 5000.0, 10000.0 };

    for (std::size_t setting = 0; setting < kSteppedPreamp.size(); ++setting)
    {
        JcmV1aStageNode node;
        node.set_preamp(kSteppedPreamp[setting]);

        // measure::render calls node.prepare() itself, so the derivation and the bias settle
        // happen there; prepared() is checked afterwards rather than preparing twice.
        const auto measured
            = SushiDSP::measure::frequency_response(node, kRunRate, frequencies);
        ASSERT_TRUE(node.prepared());
        ASSERT_EQ(measured.size(), frequencies.size());

        for (const auto& point : measured)
        {
            double expected_db = 0.0;
            ASSERT_TRUE(interpolate_magnitude(reference, static_cast<int>(setting),
                                              point.frequency_hz, expected_db))
                << "setting " << setting << " at " << point.frequency_hz << " Hz";
            EXPECT_NEAR(point.magnitude_db, expected_db, 0.5)
                << "setting " << setting << " at " << point.frequency_hz << " Hz";
        }
    }
}
