/**************************************************************************/
/* test_dk_netlist.cpp                                                    */
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
 * @file test_dk_netlist.cpp
 * @brief Tests the DK model of the jcm_v1a triode stage against a hand solve and LTspice.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp>
#include <SushiDSP/foundation/spice_reference/spice_block_compare.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>
#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/spice_reference/spice_csv.hpp>

namespace
{
    // The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp. See tests/foundation/circuits/stages/test_triode_stage/README.md.
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
        const std::array<double, 2> inputs{0.0, K_SUPPLY_VOLTS};
        std::array<double, 1> outputs{};
        const std::size_t samples = static_cast<std::size_t>(seconds * sample_rate);
        for (std::size_t index = 0; index < samples; ++index)
        {
            circuit.process_sample(port, inputs, outputs);
        }
    }

    /// @brief One instance of the D6c/D6d product-node pattern, wrapping a DK block.
    ///
    /// This adapter is the template every DK-based product module will follow: the block itself
    /// is solved entirely in double, and float appears only at the AudioNode boundary, where the
    /// host's normalised sample is scaled to volts by K_FULL_SCALE_VOLTS on the way in and back by
    /// the same constant on the way out. prepare() does all the work that is not real-time safe -
    /// the derivation and the bias settling - so process() is only the per-sample solve.
    class JcmV1aStageNode final : public SushiDSP::AudioNode
    {
        public:
            /// @brief Sets the preamp pot position; takes effect at the next prepare() or block.
            /// @param preamp Normalised preamp-pot fraction, matching the SPICE block's PREAMP.
            void set_preamp(double preamp)
            {
                preamp_ = preamp;
            }

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
                settle(circuit_, port_, info.sample_rate, K_SETTLE_SECONDS);
                circuit_.reset_statistics();
            }

            /// @brief Solves one sample per frame, casting float to volts and back.
            /// @param context The per-block buffers and frame count.
            void process(SushiDSP::ProcessContext& context) override
            {
                const float* input = context.inputs == nullptr ? nullptr : context.inputs[0];
                float* output = context.outputs[0];
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    const double drive =
                        input == nullptr ? 0.0
                                         : static_cast<double>(input[frame]) *
                                               static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS);
                    const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
                    std::array<double, 1> outputs{};
                    circuit_.process_sample(port_, inputs, outputs);
                    output[frame] = static_cast<float>(
                        outputs[0] / static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS));
                }
            }

            /// @brief Reports the node's fixed channel counts.
            /// @return One input channel and one output channel.
            SushiDSP::PortLayout ports() const override
            {
                return SushiDSP::PortLayout{1u, 1u};
            }

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
            void set_parameter_for_control_thread(const char* id, float value) override
            {
                static_cast<void>(id);
                static_cast<void>(value);
            }

            /// @brief Clears the circuit state and re-settles the bias point.
            ///
            /// Prepare-class, not real-time safe: the 200 ms settle is hundreds of thousands of
            /// Newton solves. AudioNode::reset() is documented control-thread-only, so that is
            /// legal, but a product node copying this template must keep it off the audio thread.
            void reset() override
            {
                circuit_.reset();
                settle(circuit_, port_, sample_rate_, K_SETTLE_SECONDS);
            }

            /// @brief Whether prepare() derived a usable circuit.
            /// @return True when the derivation succeeded.
            bool prepared() const
            {
                return prepared_;
            }

        private:
            static constexpr double K_SETTLE_SECONDS = 0.2;

            StageCircuit circuit_{};
            TriodeStagePort port_{};
            std::size_t pot_index_{0};
            double preamp_{0.5};
            double sample_rate_{0.0};
            bool prepared_{false};
    };

    /// @brief The three PREAMP values the SPICE block steps, in setting-index order.
    inline constexpr std::array<double, 3> K_STEPPED_PREAMP{0.1, 0.5, 0.9};
} // namespace

namespace
{
    /// @brief Run rate of the bias test; the settled bias is a DC solution, so it does not depend on the rate.
    constexpr double K_BIAS_RATE = 48000.0;

    /// @brief Silence the bias test runs before it reads the operating point.
    constexpr double K_BIAS_SETTLE_SECONDS = 0.2;

    /// @brief The circle constant, since the tests build as C++17.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Rate of the convergence run, the highest rate the stage runs at.
    constexpr double K_CONVERGENCE_RATE = 384000.0;

    /// @brief Rate of the AC sweep run.
    constexpr double K_AC_RATE = 192000.0;

    /// @brief Hand-solved cathode voltage in volts, printed to four decimals in the README.
    constexpr double K_HAND_CATHODE_VOLTS = 2.3101;

    /// @brief Hand-solved plate voltage in volts, printed to three decimals in the README.
    constexpr double K_HAND_PLATE_VOLTS = 274.442;

    /// @brief Half a unit in the last printed digit of the cathode voltage.
    constexpr double K_CATHODE_PRINT_VOLTS = 5.0e-5;

    /// @brief Half a unit in the last printed digit of the plate voltage.
    constexpr double K_PLATE_PRINT_VOLTS = 5.0e-4;

    /// @brief The relative step tolerance DampedNewtonSolver stops at, a private constant of the solver.
    constexpr double K_NEWTON_STEP_RELATIVE = 1.0e-9;

    /// @brief Largest solver variable at the bias point, the plate-to-cathode voltage, rounded up in volts.
    constexpr double K_NEWTON_ITERATE_VOLTS = 273.0;

    /// @brief The spec ceiling for the share of samples whose Newton iteration fails, in percent (spec D6b.4).
    constexpr double K_NON_CONVERGED_PERCENT = 0.01;

    /// @brief The spec ceiling for the transient null against LTspice, in decibels (spec D6b.4).
    constexpr double K_TRANSIENT_NULL_DB = -60.0;

    /// @brief The spec band for the AC match against LTspice, in decibels (spec D6b.4).
    constexpr double K_AC_BAND_DB = 0.5;

    /// @brief Passes of the 20 ms reference block that run before the compared pass.
    constexpr std::size_t K_PRE_ROLL_PASSES = 8;

    /// @brief Cutoff of the band limit applied to both transient outputs, in hertz.
    constexpr double K_NULL_BAND_HZ = 10000.0;

    /// @brief Names of the stepped PREAMP settings, in setting-index order, for the parameterised names.
    constexpr std::array<const char*, 3> K_SETTING_NAMES{"Preamp0p1", "Preamp0p5", "Preamp0p9"};

    /// @brief The operating point a settled stage reports.
    struct BiasReading
    {
            double cathode_volts{0.0};
            double plate_volts{0.0};
            double grid_to_cathode_volts{0.0};
            double grid_amps{0.0};
            std::uint64_t nan_fallbacks{0};
    };

    /// @brief Current still flowing in the coupling capacitor after a settle, an upper bound.
    ///
    /// The capacitor charges to the plate voltage through the pot and the plate network, so its
    /// current is at most the plate voltage over the two resistances, decayed by the longest time
    /// constant of that loop. Any imbalance at the plate is carried by this current alone.
    /// @param plate_volts The settled plate voltage in volts.
    /// @param seconds Silence run before the reading.
    /// @return The current in amperes.
    double coupling_residual_amps(double plate_volts, double seconds)
    {
        const double ohms = K_PREAMP_POT_OHMS + K_PLATE_OHMS;
        return plate_volts / ohms * std::exp(-seconds / (ohms * K_COUPLING_FARADS));
    }

    /// @brief Settles the stage at its middle PREAMP position and reads the operating point.
    /// @return The cathode, plate and grid readings.
    BiasReading settle_and_read_bias()
    {
        StageNetlist netlist;
        const std::size_t pot = build_jcm_v1a(netlist);

        StageCircuit circuit;
        TriodeStagePort port;
        EXPECT_TRUE(circuit.prepare(netlist, K_BIAS_RATE));
        circuit.set_potentiometer(pot, preamp_to_position(0.5));
        circuit.update_matrices_if_dirty();
        settle(circuit, port, K_BIAS_RATE, K_BIAS_SETTLE_SECONDS);

        BiasReading reading;
        const double plate_current = circuit.nonlinear_currents()[1];
        reading.grid_amps = circuit.nonlinear_currents()[0];
        reading.cathode_volts = (plate_current + reading.grid_amps) * K_CATHODE_OHMS;
        reading.plate_volts = circuit.nonlinear_voltages()[1] + reading.cathode_volts;
        reading.grid_to_cathode_volts = circuit.nonlinear_voltages()[0];
        reading.nan_fallbacks = circuit.nan_fallback_samples();
        return reading;
    }

    /// @brief Runs the 20 Hz to 20 kHz logarithmic sweep through the circuit at 384 kHz.
    void run_sweep(StageCircuit& circuit, TriodeStagePort& port)
    {
        constexpr double K_SWEEP_SECONDS = 2.0;
        constexpr double K_SWEEP_LOW_HZ = 20.0;
        constexpr double K_SWEEP_HIGH_HZ = 20000.0;
        constexpr double K_MINUS_TWELVE_DBFS = 0.25118864;
        const double amplitude = K_MINUS_TWELVE_DBFS * static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS);
        const std::size_t samples = static_cast<std::size_t>(K_SWEEP_SECONDS * K_CONVERGENCE_RATE);
        const double rate = std::log(K_SWEEP_HIGH_HZ / K_SWEEP_LOW_HZ);
        std::array<double, 1> outputs{};
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double seconds = static_cast<double>(index) / K_CONVERGENCE_RATE;
            const double phase = 2.0 * K_PI * K_SWEEP_LOW_HZ * K_SWEEP_SECONDS / rate *
                                 (std::exp(seconds * rate / K_SWEEP_SECONDS) - 1.0);
            const std::array<double, 2> inputs{amplitude * std::sin(phase), K_SUPPLY_VOLTS};
            circuit.process_sample(port, inputs, outputs);
        }
    }

    /// @brief Runs 250 ms of a 1 kHz tone at the given amplitude through the circuit at 384 kHz.
    void run_burst(StageCircuit& circuit, TriodeStagePort& port, double amplitude_volts)
    {
        constexpr double K_BURST_SECONDS = 0.25;
        constexpr double K_BURST_HZ = 1000.0;
        const std::size_t samples = static_cast<std::size_t>(K_BURST_SECONDS * K_CONVERGENCE_RATE);
        std::array<double, 1> outputs{};
        for (std::size_t index = 0; index < samples; ++index)
        {
            const double seconds = static_cast<double>(index) / K_CONVERGENCE_RATE;
            const std::array<double, 2> inputs{
                amplitude_volts * std::sin(2.0 * K_PI * K_BURST_HZ * seconds), K_SUPPLY_VOLTS};
            circuit.process_sample(port, inputs, outputs);
        }
    }

    /// @brief Builds a name for a parameterised setting row from the setting index.
    std::string setting_name(const ::testing::TestParamInfo<std::size_t>& info)
    {
        return K_SETTING_NAMES[info.param];
    }
} // namespace

/// @brief Tests that the jcm_v1a stage settles at the hand-computed bias point.
/// @see tests/foundation/circuits/stages/test_triode_stage/README.md
TEST(Small_TriodeStage, DkTriodeStageSettlesAtTheHandComputedBiasPoint)
{
    const BiasReading reading = settle_and_read_bias();

    const double residual_amps = coupling_residual_amps(K_HAND_PLATE_VOLTS, K_BIAS_SETTLE_SECONDS);
    const double newton_volts = K_NEWTON_STEP_RELATIVE * (1.0 + K_NEWTON_ITERATE_VOLTS);
    const double cathode_band = residual_amps * K_CATHODE_OHMS + K_CATHODE_PRINT_VOLTS + newton_volts;
    const double plate_band =
        residual_amps * (K_PLATE_OHMS + K_CATHODE_OHMS) + K_PLATE_PRINT_VOLTS + cathode_band;
    const double expected_grid_to_cathode = -reading.cathode_volts - reading.grid_amps * K_GRID_STOPPER_OHMS;

    EXPECT_EQ(reading.nan_fallbacks, 0u);
    EXPECT_NEAR(reading.cathode_volts, K_HAND_CATHODE_VOLTS, cathode_band);
    EXPECT_NEAR(reading.plate_volts, K_HAND_PLATE_VOLTS, plate_band);
    EXPECT_NEAR(reading.grid_to_cathode_volts, expected_grid_to_cathode,
                residual_amps * K_CATHODE_OHMS + newton_volts);
}

/// @brief Tests that the Newton solve fails on at most the spec share of the full stimulus set.
/// @see tests/foundation/circuits/stages/test_triode_stage/README.md
TEST(Large_TriodeStage, DkTriodeStageNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    StageNetlist netlist;
    const std::size_t pot = build_jcm_v1a(netlist);

    StageCircuit circuit;
    TriodeStagePort port;
    ASSERT_TRUE(circuit.prepare(netlist, K_CONVERGENCE_RATE));
    circuit.set_potentiometer(pot, preamp_to_position(0.5));
    circuit.update_matrices_if_dirty();
    settle(circuit, port, K_CONVERGENCE_RATE, K_BIAS_SETTLE_SECONDS);
    circuit.reset_statistics();

    run_sweep(circuit, port);
    for (const double amplitude_volts : {0.05, 0.5, 5.0})
    {
        run_burst(circuit, port, amplitude_volts);
    }

    const double percent = 100.0 * static_cast<double>(circuit.non_converged_samples()) /
                           static_cast<double>(circuit.total_samples());
    EXPECT_LE(percent, K_NON_CONVERGED_PERCENT);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

/// @brief Loads the LTspice transient reference once per test and skips the test when it is absent.
class Medium_TriodeStageSpiceTransient : public ::testing::TestWithParam<std::size_t>
{
    protected:
        void SetUp() override
        {
            const std::string path = spice_path("jcm_v1a", "tran");
            reference_ = SushiDSP::measure::load_transient_csv(path);
            if (reference_.input.empty())
            {
                GTEST_SKIP() << "No LTspice reference at " << path
                             << ". Generate it with: python tools/spice/export.py --block jcm_v1a "
                             << "(needs LTspice installed; see docs/reference/spice/README.md), "
                             << "then commit the CSV.";
            }
        }

        /// @brief Runs the stage over one setting's block and returns its band-limited output.
        /// @param setting The setting index.
        /// @param reference_output Receives the band-limited LTspice output for the setting.
        /// @return The band-limited output of the DK stage for the setting.
        std::vector<float> run_setting(std::size_t setting, std::vector<float>& reference_output) const
        {
            StageNetlist netlist;
            const std::size_t pot = build_jcm_v1a(netlist);

            StageCircuit circuit;
            TriodeStagePort port;
            EXPECT_TRUE(circuit.prepare(netlist, reference_.sample_rate));
            circuit.set_potentiometer(pot, preamp_to_position(K_STEPPED_PREAMP[setting]));
            circuit.update_matrices_if_dirty();
            settle(circuit, port, reference_.sample_rate, K_BIAS_SETTLE_SECONDS);

            std::vector<double> stimulus;
            for (std::size_t row = 0; row < reference_.input.size(); ++row)
            {
                if (reference_.setting[row] == static_cast<int>(setting))
                {
                    stimulus.push_back(static_cast<double>(reference_.input[row]));
                    reference_output.push_back(reference_.output[row]);
                }
            }

            std::vector<float> circuit_output;
            std::array<double, 1> outputs{};
            for (std::size_t pass = 0; pass <= K_PRE_ROLL_PASSES; ++pass)
            {
                for (const double drive : stimulus)
                {
                    const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
                    circuit.process_sample(port, inputs, outputs);
                    if (pass == K_PRE_ROLL_PASSES)
                    {
                        circuit_output.push_back(static_cast<float>(outputs[0]));
                    }
                }
            }
            band_limit(reference_output, reference_.sample_rate, K_NULL_BAND_HZ);
            band_limit(circuit_output, reference_.sample_rate, K_NULL_BAND_HZ);
            return circuit_output;
        }

        SushiDSP::measure::Transient reference_{};
};

/// @brief Tests that the DK stage nulls against the LTspice transient at each stepped setting.
/// @see tests/foundation/circuits/stages/test_triode_stage/README.md
TEST_P(Medium_TriodeStageSpiceTransient, DkTriodeStageNullsAgainstTheLtspiceTransientReference)
{
    ASSERT_GT(reference_.sample_rate, 0.0);

    std::vector<float> reference_output;
    const std::vector<float> circuit_output = run_setting(GetParam(), reference_output);
    ASSERT_FALSE(reference_output.empty());

    const double residual = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
    EXPECT_LE(residual, K_TRANSIENT_NULL_DB);
}

INSTANTIATE_TEST_SUITE_P(Settings, Medium_TriodeStageSpiceTransient, ::testing::Values<std::size_t>(0, 1, 2),
                         setting_name);

/// @brief Loads the LTspice AC reference once per test and skips the test when it is absent.
class Medium_TriodeStageSpiceAc : public ::testing::TestWithParam<std::size_t>
{
    protected:
        void SetUp() override
        {
            const std::string path = spice_path("jcm_v1a", "ac");
            reference_ = SushiDSP::measure::load_ac_csv(path);
            if (reference_.frequency_hz.empty())
            {
                GTEST_SKIP() << "No LTspice reference at " << path
                             << ". Generate it with: python tools/spice/export.py --block jcm_v1a "
                             << "(needs LTspice installed; see docs/reference/spice/README.md), "
                             << "then commit the CSV.";
            }
        }

        SushiDSP::measure::AcSweep reference_{};
};

/// @brief Tests that the DK stage matches the LTspice AC sweep within the spec band at each setting.
/// @see tests/foundation/circuits/stages/test_triode_stage/README.md
TEST_P(Medium_TriodeStageSpiceAc, DkTriodeStageMatchesTheLtspiceAcSweepWithinHalfADecibel)
{
    // -40 dBFS keeps the stage small-signal. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const std::vector<double> frequencies{20.0,   50.0,   100.0,  200.0,  500.0,
                                          1000.0, 2000.0, 5000.0, 10000.0};
    const std::size_t setting = GetParam();

    JcmV1aStageNode node;
    node.set_preamp(K_STEPPED_PREAMP[setting]);

    // measure::render calls node.prepare() itself. See tests/foundation/circuits/stages/test_triode_stage/README.md.
    const auto measured = SushiDSP::measure::frequency_response(node, K_AC_RATE, frequencies);
    ASSERT_TRUE(node.prepared());
    ASSERT_EQ(measured.size(), frequencies.size());

    for (const auto& point : measured)
    {
        double expected_db = 0.0;
        ASSERT_TRUE(interpolate_magnitude(reference_, static_cast<int>(setting), point.frequency_hz, expected_db))
            << "at " << point.frequency_hz << " Hz";
        EXPECT_NEAR(point.magnitude_db, expected_db, K_AC_BAND_DB) << "at " << point.frequency_hz << " Hz";
    }
}

INSTANTIATE_TEST_SUITE_P(Settings, Medium_TriodeStageSpiceAc, ::testing::Values<std::size_t>(0, 1, 2), setting_name);
