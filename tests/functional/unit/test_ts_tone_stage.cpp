/**************************************************************************/
/* test_ts_tone_stage.cpp                                                 */
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
 * @file test_ts_tone_stage.cpp
 * @brief Tests for the Tube Screamer tone block on the DK core.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp>

#include "circuits/dk_block_node.hpp"
#include "circuits/spice_block_compare.hpp"
#include "measure/frequency_response.hpp"
#include "measure/null_test.hpp"
#include "measure/spice_csv.hpp"

namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ToneNode = DkBlockNode<ToneNetlist, ToneCircuit, LinearPort>;

    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The three TONE values ts_tone.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> K_STEPPED_TONE{0.1, 0.5, 0.9};

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
} // namespace

TEST(Unit_Dsp, DkTsToneBlockResponseMatchesItsClosedForm)
{
    // The 0.25 dB tolerance is the sweep's own accuracy. See tests/functional/unit/README.md.
    constexpr double K_RUN_RATE = 384000.0;
    const std::vector<double> frequencies{20.0,   50.0,   100.0,  200.0,  500.0,
                                          1000.0, 2000.0, 5000.0, 10000.0};

    for (const double tone : K_STEPPED_TONE)
    {
        ToneNode node;
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = tone;
        node.pot_positions[pots.level] = K_LEVEL_POT_POSITION;

        const auto measured = SushiDSP::measure::frequency_response(node, K_RUN_RATE, frequencies);
        ASSERT_TRUE(node.prepared());
        ASSERT_EQ(measured.size(), frequencies.size());
        for (const auto& point : measured)
        {
            EXPECT_NEAR(point.magnitude_db, tone_closed_form_db(point.frequency_hz, tone), 0.25)
                << tone << " at " << point.frequency_hz << " Hz";
        }
    }
}

TEST(Unit_Dsp, DkTsToneBlockClosedFormCostsUnderOneDecibelAtOneHundredHertzAtNoon)
{
    // The stage costs under 1 dB at 100 Hz at noon. See tests/functional/unit/README.md.
    EXPECT_NEAR(tone_closed_form_db(100.0, 0.5), -0.9946, 0.001);
    EXPECT_GT(tone_closed_form_db(100.0, 0.5), -1.0);
}

TEST(Unit_Dsp, DkTsToneBlockClosedFormSweepsSixteenDecibelsAtTheLegsTurnoverFrequency)
{
    // The wiper leg's turnover sits at about 3.2 kHz. See tests/functional/unit/README.md.
    const double dark = tone_closed_form_db(3200.0, 0.0 + 1.0e-9);
    const double bright = tone_closed_form_db(3200.0, 1.0 - 1.0e-9);
    EXPECT_NEAR(bright - dark, 16.185, 0.01);

    // At the bright end the stage passes +0.435 dB at 1 kHz. See tests/functional/unit/README.md.
    EXPECT_GT(tone_closed_form_db(1000.0, 1.0 - 1.0e-9), 0.0);
}

TEST(Unit_Dsp, DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon)
{
    // The real network cannot notch above 723 Hz. See tests/functional/unit/README.md.
    constexpr double K_RUN_RATE = 384000.0;
    const std::vector<double> frequencies = midband_frequencies();

    for (std::size_t index = 1; index < frequencies.size(); ++index)
    {
        EXPECT_LE(tone_closed_form_db(frequencies[index], 0.5),
                  tone_closed_form_db(frequencies[index - 1], 0.5) + 1.0e-9)
            << frequencies[index];
    }

    ToneNode node;
    const TonePotentiometers pots = build_tone_block(node.netlist);
    node.pot_positions[pots.tone] = 0.5;
    node.pot_positions[pots.level] = K_LEVEL_POT_POSITION;

    const auto measured = SushiDSP::measure::frequency_response(node, K_RUN_RATE, frequencies);
    ASSERT_TRUE(node.prepared());
    ASSERT_EQ(measured.size(), frequencies.size());
    for (std::size_t index = 1; index < measured.size(); ++index)
    {
        EXPECT_LE(measured[index].magnitude_db, measured[index - 1].magnitude_db + 0.25)
            << measured[index].frequency_hz;
    }
}

TEST(Unit_Dsp, DkTsToneBlockKnobMapsOntoTheReferencesSteppedRange)
{
    // TONE = 1 is bright and the mapping is the identity. See tests/functional/unit/README.md.
    EXPECT_DOUBLE_EQ(tone_to_position(0.0), 0.0);
    EXPECT_DOUBLE_EQ(tone_to_position(0.5), 0.5);
    EXPECT_DOUBLE_EQ(tone_to_position(1.0), 1.0);
}

TEST(Unit_Dsp, DkTsToneBlockPreparesAtEveryTonePositionIncludingBothEndStops)
{
    // Both end stops are ordinary settings. See tests/functional/unit/README.md.
    ToneNetlist netlist;
    const TonePotentiometers pots = build_tone_block(netlist);
    EXPECT_EQ(netlist.node_count, 9u);

    ToneCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 384000.0));
    for (const double position : {0.0, 0.1, 0.5, 0.9, 1.0})
    {
        circuit.set_potentiometer(pots.tone, position);
        circuit.set_potentiometer(pots.level, K_LEVEL_POT_POSITION);
        circuit.update_matrices_if_dirty();

        LinearPort port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, {0.1}, outputs);
            ASSERT_TRUE(std::isfinite(outputs[0])) << position;
        }
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsToneBlockMatchesTheLtspiceAcSweepWithinHalfADecibel)
{
    if (!K_SPICE_REFERENCE_MATCHES_SCHEMATIC)
    {
        GTEST_SKIP() << "ts_tone_ac.csv was exported from the netlist the 2026-09-06 correction "
                     << "replaced. Regenerate it with: " << K_REGENERATE_COMMAND
                     << ", commit the CSV, and set K_SPICE_REFERENCE_MATCHES_SCHEMATIC to true.";
    }

    const std::string path = spice_path("ts_tone", "ac");
    const auto reference = SushiDSP::measure::load_ac_csv(path);
    if (reference.frequency_hz.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: " << K_REGENERATE_COMMAND << ", then commit the CSV.";
    }

    constexpr double K_RUN_RATE = 192000.0;
    const std::vector<double> frequencies{20.0,   50.0,   100.0,  200.0,  500.0,
                                          1000.0, 2000.0, 5000.0, 10000.0};

    for (std::size_t setting = 0; setting < K_STEPPED_TONE.size(); ++setting)
    {
        ToneNode node;
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = K_STEPPED_TONE[setting];
        node.pot_positions[pots.level] = K_LEVEL_POT_POSITION;

        const auto measured = SushiDSP::measure::frequency_response(node, K_RUN_RATE, frequencies);
        ASSERT_TRUE(node.prepared());
        for (const auto& point : measured)
        {
            double expected = 0.0;
            ASSERT_TRUE(interpolate_magnitude(reference, static_cast<int>(setting),
                                              point.frequency_hz, expected))
                << "setting " << setting << " at " << point.frequency_hz << " Hz";
            EXPECT_NEAR(point.magnitude_db, expected, 0.5)
                << "setting " << setting << " at " << point.frequency_hz << " Hz";
        }
    }
}

TEST(Unit_Dsp, DkTsToneBlockNullsAgainstTheLtspiceTransientReference)
{
    if (!K_SPICE_REFERENCE_MATCHES_SCHEMATIC)
    {
        GTEST_SKIP() << "ts_tone_tran.csv was exported from the netlist the 2026-09-06 "
                     << "correction replaced. Regenerate it with: " << K_REGENERATE_COMMAND
                     << ", commit the CSV, and set K_SPICE_REFERENCE_MATCHES_SCHEMATIC to true.";
    }

    const std::string path = spice_path("ts_tone", "tran");
    const auto reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: " << K_REGENERATE_COMMAND << ", then commit the CSV.";
    }

    ASSERT_GT(reference.sample_rate, 0.0);

    for (std::size_t setting = 0; setting < K_STEPPED_TONE.size(); ++setting)
    {
        ToneNetlist netlist;
        const TonePotentiometers pots = build_tone_block(netlist);

        ToneCircuit circuit;
        LinearPort port;
        ASSERT_TRUE(circuit.prepare(netlist, reference.sample_rate));
        circuit.set_potentiometer(pots.tone, K_STEPPED_TONE[setting]);
        circuit.set_potentiometer(pots.level, K_LEVEL_POT_POSITION);
        circuit.update_matrices_if_dirty();

        // One pre-roll pass matches LTspice's own start. See tests/functional/unit/README.md.
        constexpr std::size_t K_PRE_ROLL_PASSES = 1;

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
        ASSERT_FALSE(reference_output.empty()) << "setting " << setting;

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

        const double residual = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
        std::printf("[ measure ] ts_tone tran setting %zu: %.2f dB residual\n", setting, residual);
        EXPECT_LE(residual, -60.0) << "setting " << setting;
    }
}
