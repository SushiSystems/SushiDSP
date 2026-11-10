/**************************************************************************/
/* test_tone_stage.cpp                                                    */
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
 * @file test_tone_stage.cpp
 * @brief Tests for the Tube Screamer tone block on the DK core.
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
#include <optional>
#include <string>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp>

#include <SushiDSP/foundation/circuit_blocks/dk_block_node.hpp>
#include <SushiDSP/foundation/spice_reference/spice_block_compare.hpp>
#include <SushiDSP/foundation/signal_measure/frequency_response.hpp>
#include <SushiDSP/foundation/signal_measure/null_test.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/foundation/signal_measure/spectrum.hpp>
#include <SushiDSP/foundation/spice_reference/spice_csv.hpp>

namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ToneNode = DkBlockNode<ToneNetlist, ToneCircuit, LinearPort>;

    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The three TONE values ts_tone.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> K_STEPPED_TONE{0.1, 0.5, 0.9};

    /// @brief The sweep's own accuracy, in dB. Characterized (README): the discrete-time block at
    /// 384 kHz tracks the continuous closed form to within this over 20 Hz to 10 kHz.
    constexpr double K_SWEEP_ACCURACY_DB = 0.25;

    /// @brief The spread, in dB, between the bright and the dark end of the tone pot at the wiper
    /// leg's turnover frequency. Specified (README): the closed form gives 16.185 dB at 3.2 kHz.
    constexpr double K_TURNOVER_SPREAD_DB = 16.185;

    /// @brief The turnover frequency of the wiper leg, in hertz (README).
    constexpr double K_TURNOVER_HZ = 3200.0;

    /// @brief The deviation, in dB, the block may show against the LTspice AC sweep. Characterized:
    /// the figure the committed reference was held to.
    constexpr double K_AC_MATCH_BOUND_DB = 0.5;

    /// @brief The null residual, in dB, the block must reach against the LTspice transient.
    /// Characterized: the figure the committed reference was held to.
    constexpr double K_TRANSIENT_NULL_BOUND_DB = -60.0;

    /// @brief Whether the committed ts_tone CSVs still describe ts_tone.asc.
    ///
    /// They do not. The 2026-09-06 correction replaced the netlist - the passive filter was
    /// inverted, the tone pot bridged the wrong nodes, the 220 Ohm / 220 nF wiper leg was
    /// missing and the op-amp was inverting - so the CSVs 2bdbf91 committed describe a circuit
    /// that no longer exists. docs/reference/spice/ts_tone/README.md carries the evidence.
    /// Flip this to true in the same commit that lands the regenerated CSVs.
    inline constexpr bool K_SPICE_REFERENCE_MATCHES_SCHEMATIC = false;

    /// @brief The regeneration command the two SPICE cases name when they skip.
    inline constexpr const char* K_REGENERATE_COMMAND =
        "python tools/spice/export.py ts_tone "
        "--ltspice \"%LOCALAPPDATA%/Programs/ADI/LTspice/LTspice.exe\"";

    /// @brief The frequencies the sweeps measure at, in hertz.
    const std::vector<double> K_SWEEP_FREQUENCIES{20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 3200.0, 5000.0, 10000.0};

    /// @brief The closed-form response of the corrected tone stage.
    ///
    /// Both pot ends sit on op-amp input nodes that draw no current, so the wiper leg sees one
    /// Thevenin source and takes `share` of it: the + -side section loads the passive filter by
    /// `upper * share` and the - -side section carries `lower * share` into the gain leg.
    /// @param frequency_hz Frequency in hertz.
    /// @param tone The tone pot's position, which is the reference's TONE.
    /// @return Magnitude in decibels.
    double tone_closed_form_db(double frequency_hz, double tone)
    {
        const double series = 1.0 / K_INPUT_SERIES_OHMS;             // G7
        const double bias = 1.0 / K_BIAS_RETURN_OHMS;                // G9
        const double upper = 1.0 / (K_TONE_POT_OHMS * tone);         // Ga
        const double lower = 1.0 / (K_TONE_POT_OHMS * (1.0 - tone)); // Gb
        const std::complex<double> s(0.0, 2.0 * K_PI * frequency_hz);
        const std::complex<double> leg =
            s * K_TONE_LEG_FARADS / (1.0 + s * K_TONE_LEG_OHMS * K_TONE_LEG_FARADS);
        const std::complex<double> share = leg / (upper + lower + leg);
        const std::complex<double> filter =
            series / (series + bias + s * K_INPUT_SHUNT_FARADS + upper * share);
        const std::complex<double> gain = 1.0 + lower * share * K_FEEDBACK_OHMS;
        const std::complex<double> output =
            s * K_OUTPUT_FARADS * K_LEVEL_POT_OHMS /
            (1.0 + s * K_OUTPUT_FARADS * (K_LEVEL_POT_OHMS + K_OUTPUT_SERIES_OHMS));
        return 20.0 * std::log10(std::abs(filter * gain * output));
    }

    /// @brief Twenty-four logarithmically spaced frequencies from 300 Hz to 2 kHz.
    /// @return The frequency list.
    std::vector<double> midband_frequencies()
    {
        std::vector<double> frequencies;
        for (std::size_t index = 0; index < 24; ++index)
        {
            frequencies.push_back(300.0 *
                                  std::pow(2000.0 / 300.0, static_cast<double>(index) / 23.0));
        }
        return frequencies;
    }

    /// @brief Builds the tone block's netlist into a node and sets the tone pot and the level pot.
    /// @param node The node to fill.
    /// @param tone The tone pot's position.
    void place_pots(ToneNode& node, double tone)
    {
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = tone;
        node.pot_positions[pots.level] = K_LEVEL_POT_POSITION;
    }

    /// @brief Counts the frames a tone is rendered before the analysis frame opens, at one rate:
    /// ten time constants of the slowest section the frame can see, the wiper leg through the
    /// whole 20 kOhm pot, 220 nF x (220 Ohm + 20 kOhm) = 4.45 ms, which leaves e^-10 = 4.5e-5 of
    /// a start-up transient, -87 dB. The output coupling's 0.1 s is left out: its start-up is a
    /// decaying offset that the Blackman-Harris frame, 92 dB down away from its own bin, does not
    /// read at 3.2 kHz.
    /// @param run_rate The rate the block runs at.
    std::size_t settle_frames(double run_rate)
    {
        constexpr double K_SETTLE_TIME_CONSTANTS = 10.0;
        return static_cast<std::size_t>(K_SETTLE_TIME_CONSTANTS * K_TONE_LEG_FARADS *
                                        (K_TONE_LEG_OHMS + K_TONE_POT_OHMS) * run_rate) +
               1;
    }

    /// @brief Measures the level in dB the block gives one bin-exact tone at a tone position. The
    /// stimulus is the sweep's -40 dBFS, so two positions read through one call compare by their
    /// difference.
    /// @param tone The tone pot's position.
    /// @param run_rate The rate the block runs at.
    /// @param tone_hz The tone to render; it is moved to the nearest bin-exact frequency.
    /// @return The level of the fundamental in the analysis frame, or nothing when the block did not prepare.
    std::optional<double> tone_level_db(double tone, double run_rate, double tone_hz)
    {
        constexpr float K_AMPLITUDE = 0.01f;
        ToneNode node;
        place_pots(node, tone);

        const std::size_t settle = settle_frames(run_rate);
        const double f0 = SushiDSP::measure::bin_exact_frequency(tone_hz, run_rate, SushiDSP::measure::K_FFT_SIZE);
        const auto output = SushiDSP::measure::render(
            node, SushiDSP::measure::sine_stimulus(f0, run_rate, settle + SushiDSP::measure::K_FFT_SIZE, K_AMPLITUDE),
            run_rate, 256);
        if (!node.prepared())
        {
            return std::nullopt;
        }
        const auto spectrum = SushiDSP::measure::magnitude_spectrum_db(output, settle);
        return SushiDSP::measure::peak_near(spectrum, SushiDSP::measure::bin_of(f0, run_rate));
    }

    /// @brief Measures the block's magnitude response at a tone position and the level pot's reference position.
    /// @param tone The tone pot's position.
    /// @param run_rate The rate the block runs at.
    /// @param frequencies The frequencies to measure at.
    /// @return One point per frequency; empty when the block did not prepare.
    std::vector<SushiDSP::measure::ResponsePoint> measure_tone_response(double tone, double run_rate,
                                                                        const std::vector<double>& frequencies)
    {
        ToneNode node;
        place_pots(node, tone);

        auto measured = SushiDSP::measure::frequency_response(node, run_rate, frequencies);
        if (!node.prepared())
        {
            measured.clear();
        }
        return measured;
    }

    /// @brief Returns the worst deviation from the closed form over the measured points, in dB.
    double worst_deviation_from_closed_form_db(const std::vector<SushiDSP::measure::ResponsePoint>& measured, double tone)
    {
        double worst = 0.0;
        for (const auto& point : measured)
        {
            worst = std::max(worst, std::fabs(point.magnitude_db - tone_closed_form_db(point.frequency_hz, tone)));
        }
        return worst;
    }

    /// @brief Returns the largest rise, in dB, from one measured point to the next; negative when
    /// every step falls.
    double largest_rise_db(const std::vector<SushiDSP::measure::ResponsePoint>& measured)
    {
        double largest = -std::numeric_limits<double>::infinity();
        for (std::size_t index = 1; index < measured.size(); ++index)
        {
            largest = std::max(largest, measured[index].magnitude_db - measured[index - 1].magnitude_db);
        }
        return largest;
    }

    /// @brief Steps the block to a tone position, runs 64 samples of 0.1 V and reports what came out.
    struct PositionRun
    {
            bool prepared = false;
            bool finite = true;
            std::uint64_t nan_fallbacks = 0;
    };

    /// @brief Runs the block over every position in the list and reports the whole run.
    PositionRun run_at_position(double position)
    {
        PositionRun run;
        ToneNetlist netlist;
        const TonePotentiometers pots = build_tone_block(netlist);
        ToneCircuit circuit;
        run.prepared = circuit.prepare(netlist, 384000.0);
        circuit.set_potentiometer(pots.tone, position);
        circuit.set_potentiometer(pots.level, K_LEVEL_POT_POSITION);
        circuit.update_matrices_if_dirty();

        LinearPort port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, {0.1}, outputs);
            run.finite = run.finite && std::isfinite(outputs[0]);
        }
        run.nan_fallbacks = circuit.nan_fallback_samples();
        return run;
    }

    /// @brief Returns the worst deviation, in dB, of the block from the LTspice AC sweep at one setting,
    /// or infinity when the sweep has no value for a frequency.
    double worst_deviation_from_ac_reference_db(const SushiDSP::measure::AcSweep& reference, std::size_t setting)
    {
        constexpr double K_RUN_RATE = 192000.0;
        const auto measured = measure_tone_response(K_STEPPED_TONE[setting], K_RUN_RATE, K_SWEEP_FREQUENCIES);
        double worst = measured.empty() ? std::numeric_limits<double>::infinity() : 0.0;
        for (const auto& point : measured)
        {
            double expected = 0.0;
            if (!interpolate_magnitude(reference, static_cast<int>(setting), point.frequency_hz, expected))
            {
                return std::numeric_limits<double>::infinity();
            }
            worst = std::max(worst, std::fabs(point.magnitude_db - expected));
        }
        return worst;
    }

    /// @brief Returns the null residual, in dB, of the block against the LTspice transient at one
    /// setting, or 0 dB when the setting has no rows or the block does not prepare.
    double transient_null_residual_db(const SushiDSP::measure::Transient& reference, std::size_t setting)
    {
        ToneNetlist netlist;
        const TonePotentiometers pots = build_tone_block(netlist);

        ToneCircuit circuit;
        LinearPort port;
        if (!circuit.prepare(netlist, reference.sample_rate))
        {
            return 0.0;
        }
        circuit.set_potentiometer(pots.tone, K_STEPPED_TONE[setting]);
        circuit.set_potentiometer(pots.level, K_LEVEL_POT_POSITION);
        circuit.update_matrices_if_dirty();

        std::vector<double> stimulus;
        std::vector<float> reference_output;
        for (std::size_t row = 0; row < reference.input.size(); ++row)
        {
            if (reference.setting[row] == static_cast<int>(setting))
            {
                stimulus.push_back(static_cast<double>(reference.input[row]));
                reference_output.push_back(reference.output[row]);
            }
        }
        if (reference_output.empty())
        {
            return 0.0;
        }

        // One pre-roll pass matches LTspice's own start. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
        constexpr std::size_t K_PRE_ROLL_PASSES = 1;
        std::vector<float> circuit_output;
        std::array<double, 1> outputs{};
        for (std::size_t pass = 0; pass <= K_PRE_ROLL_PASSES; ++pass)
        {
            const bool final_pass = pass == K_PRE_ROLL_PASSES;
            for (std::size_t index = 0; index < stimulus.size(); ++index)
            {
                circuit.process_sample(port, {stimulus[index]}, outputs);
                if (final_pass)
                {
                    circuit_output.push_back(static_cast<float>(outputs[0]));
                }
            }
        }

        band_limit(reference_output, reference.sample_rate, 10000.0);
        band_limit(circuit_output, reference.sample_rate, 10000.0);
        return SushiDSP::measure::null_test(reference_output, circuit_output, 256);
    }

    /// @brief Names a pot position.
    struct PositionRow
    {
            const char* name;
            double position;
    };

    /// @brief Names a setting index of the stepped reference.
    struct SettingRow
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

class Medium_TubeScreamerCircuitsToneResponse : public ::testing::TestWithParam<PositionRow>
{
};

TEST_P(Medium_TubeScreamerCircuitsToneResponse, DkTsToneBlockResponseMatchesItsClosedForm)
{
    // The 0.25 dB tolerance is the sweep's own accuracy. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr double K_RUN_RATE = 384000.0;
    const auto measured = measure_tone_response(GetParam().position, K_RUN_RATE, K_SWEEP_FREQUENCIES);

    ASSERT_EQ(measured.size(), K_SWEEP_FREQUENCIES.size());
    EXPECT_LE(worst_deviation_from_closed_form_db(measured, GetParam().position), K_SWEEP_ACCURACY_DB);
}

INSTANTIATE_TEST_SUITE_P(Rows, Medium_TubeScreamerCircuitsToneResponse,
                         ::testing::Values(PositionRow{"Tone0p1", K_STEPPED_TONE[0]}, PositionRow{"Tone0p5", K_STEPPED_TONE[1]},
                                           PositionRow{"Tone0p9", K_STEPPED_TONE[2]}),
                         row_name<PositionRow>);

TEST(Medium_TubeScreamerCircuitsTone, DkTsToneBlockBrightEndExceedsTheDarkEndAtTheLegsTurnoverFrequency)
{
    // The wiper leg's turnover sits at about 3.2 kHz. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr double K_RUN_RATE = 384000.0;
    const auto dark = tone_level_db(0.0, K_RUN_RATE, K_TURNOVER_HZ);
    const auto bright = tone_level_db(1.0, K_RUN_RATE, K_TURNOVER_HZ);

    ASSERT_TRUE(dark.has_value());
    ASSERT_TRUE(bright.has_value());
    EXPECT_NEAR(*bright - *dark, K_TURNOVER_SPREAD_DB, K_SWEEP_ACCURACY_DB);
}

TEST(Medium_TubeScreamerCircuitsTone, DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon)
{
    // The real network cannot notch above 723 Hz. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    constexpr double K_RUN_RATE = 384000.0;
    const std::vector<double> frequencies = midband_frequencies();
    const auto measured = measure_tone_response(0.5, K_RUN_RATE, frequencies);

    ASSERT_EQ(measured.size(), frequencies.size());
    EXPECT_LE(largest_rise_db(measured), K_SWEEP_ACCURACY_DB);
}

class Small_TubeScreamerCircuitsTonePosition : public ::testing::TestWithParam<PositionRow>
{
};

TEST_P(Small_TubeScreamerCircuitsTonePosition, DkTsToneBlockStaysFiniteAtThePosition)
{
    // Both end stops are ordinary settings. See tests/nodes/tubescreamer/test_tube_screamer_circuits/README.md.
    const PositionRun run = run_at_position(GetParam().position);

    ASSERT_TRUE(run.prepared);
    EXPECT_TRUE(run.finite);
}

TEST_P(Small_TubeScreamerCircuitsTonePosition, DkTsToneBlockNeverFallsBackToNanAtThePosition)
{
    const PositionRun run = run_at_position(GetParam().position);

    ASSERT_TRUE(run.prepared);
    EXPECT_EQ(run.nan_fallbacks, 0u);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsTonePosition,
                         ::testing::Values(PositionRow{"Dark", 0.0}, PositionRow{"Tone0p1", 0.1}, PositionRow{"Tone0p5", 0.5},
                                           PositionRow{"Tone0p9", 0.9}, PositionRow{"Bright", 1.0}),
                         row_name<PositionRow>);

/// @brief Skips its tests until the committed ts_tone reference describes the schematic again.
class Small_TubeScreamerCircuitsToneReference : public ::testing::TestWithParam<SettingRow>
{
    protected:
        void SetUp() override
        {
            if (!K_SPICE_REFERENCE_MATCHES_SCHEMATIC)
            {
                GTEST_SKIP() << "the ts_tone CSVs were exported from the netlist the 2026-09-06 correction "
                             << "replaced. Regenerate them with: " << K_REGENERATE_COMMAND
                             << ", commit the CSVs, and set K_SPICE_REFERENCE_MATCHES_SCHEMATIC to true.";
            }
        }
};

/// @brief Loads the AC reference, and skips when the file is absent.
class Small_TubeScreamerCircuitsToneAc : public Small_TubeScreamerCircuitsToneReference
{
    protected:
        void SetUp() override
        {
            Small_TubeScreamerCircuitsToneReference::SetUp();
            if (IsSkipped())
            {
                return;
            }
            reference_ = SushiDSP::measure::load_ac_csv(spice_path("ts_tone", "ac"));
            if (reference_.frequency_hz.empty())
            {
                GTEST_SKIP() << "No LTspice AC reference. Generate it with: " << K_REGENERATE_COMMAND
                             << ", then commit the CSV.";
            }
        }

        SushiDSP::measure::AcSweep reference_;
};

/// @brief Loads the transient reference, and skips when the file is absent.
class Small_TubeScreamerCircuitsToneTransient : public Small_TubeScreamerCircuitsToneReference
{
    protected:
        void SetUp() override
        {
            Small_TubeScreamerCircuitsToneReference::SetUp();
            if (IsSkipped())
            {
                return;
            }
            reference_ = SushiDSP::measure::load_transient_csv(spice_path("ts_tone", "tran"));
            if (reference_.input.empty())
            {
                GTEST_SKIP() << "No LTspice transient reference. Generate it with: " << K_REGENERATE_COMMAND
                             << ", then commit the CSV.";
            }
        }

        SushiDSP::measure::Transient reference_;
};

TEST_P(Small_TubeScreamerCircuitsToneAc, DkTsToneBlockMatchesTheLtspiceAcSweep)
{
    EXPECT_LE(worst_deviation_from_ac_reference_db(reference_, GetParam().setting), K_AC_MATCH_BOUND_DB);
}

TEST_P(Small_TubeScreamerCircuitsToneTransient, DkTsToneBlockNullsAgainstTheLtspiceTransientReference)
{
    ASSERT_GT(reference_.sample_rate, 0.0);

    EXPECT_LE(transient_null_residual_db(reference_, GetParam().setting), K_TRANSIENT_NULL_BOUND_DB);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsToneAc,
                         ::testing::Values(SettingRow{"Setting0", 0}, SettingRow{"Setting1", 1}, SettingRow{"Setting2", 2}),
                         row_name<SettingRow>);

INSTANTIATE_TEST_SUITE_P(Rows, Small_TubeScreamerCircuitsToneTransient,
                         ::testing::Values(SettingRow{"Setting0", 0}, SettingRow{"Setting1", 1}, SettingRow{"Setting2", 2}),
                         row_name<SettingRow>);
