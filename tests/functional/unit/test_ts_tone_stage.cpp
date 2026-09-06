/**************************************************************************/
/* test_ts_tone_stage.cpp                                                 */
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

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/tubescreamer/TubeScreamerCircuits.hpp"

#include "circuits/DkBlockNode.hpp"
#include "circuits/SpiceBlockCompare.hpp"
#include "measure/FrequencyResponse.hpp"
#include "measure/NullTest.hpp"
#include "measure/SpiceCsv.hpp"

namespace
{
    using namespace SushiDSP::tube_screamer_circuits;
    using namespace SushiDSP::test_circuits;

    using ToneNode = DkBlockNode<ToneNetlist, ToneCircuit, LinearPort>;

    /// @brief Pi to double precision, spelled out so the tests do not depend on M_PI.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief The three TONE values ts_tone.asc steps, in setting-index order.
    inline constexpr std::array<double, 3> kSteppedTone{ 0.1, 0.5, 0.9 };

    /// @brief Whether the committed ts_tone CSVs still describe ts_tone.asc.
    ///
    /// They do not. The 2026-09-06 correction replaced the netlist - the passive filter was
    /// inverted, the tone pot bridged the wrong nodes, the 220 Ohm / 220 nF wiper leg was
    /// missing and the op-amp was inverting - so the CSVs 2bdbf91 committed describe a circuit
    /// that no longer exists. docs/reference/spice/ts_tone/README.md carries the evidence.
    /// Flip this to true in the same commit that lands the regenerated CSVs.
    inline constexpr bool kSpiceReferenceMatchesSchematic = false;

    /// @brief The regeneration command the two SPICE cases name when they skip.
    inline constexpr const char* kRegenerateCommand
        = "python tools/spice/export.py ts_tone "
          "--ltspice \"%LOCALAPPDATA%/Programs/ADI/LTspice/LTspice.exe\"";

    /// @brief The closed-form response of the corrected tone stage.
    ///
    /// Both pot ends sit on op-amp input nodes that draw no current, so the wiper leg sees one
    /// Thevenin source and takes `share` of it; the + -side section then loads the passive
    /// filter by `upper * share` and the - -side section carries `lower * share` into the gain
    /// leg, which is where the bright end's treble boost comes from. C7 into the 1 kOhm and the
    /// level pot adds the output high-pass.
    /// @param frequency_hz Frequency in hertz.
    /// @param tone The tone pot's position, which is the reference's TONE.
    /// @return Magnitude in decibels.
    double tone_closed_form_db(double frequency_hz, double tone)
    {
        const double series = 1.0 / kInputSeriesOhms;               // G7
        const double bias = 1.0 / kBiasReturnOhms;                  // G9
        const double upper = 1.0 / (kTonePotOhms * tone);           // Ga
        const double lower = 1.0 / (kTonePotOhms * (1.0 - tone));   // Gb
        const std::complex<double> s(0.0, 2.0 * kPi * frequency_hz);
        const std::complex<double> leg
            = s * kToneLegFarads / (1.0 + s * kToneLegOhms * kToneLegFarads);
        const std::complex<double> share = leg / (upper + lower + leg);
        const std::complex<double> filter
            = series / (series + bias + s * kInputShuntFarads + upper * share);
        const std::complex<double> gain = 1.0 + lower * share * kFeedbackOhms;
        const std::complex<double> output
            = s * kOutputFarads * kLevelPotOhms
              / (1.0 + s * kOutputFarads * (kLevelPotOhms + kOutputSeriesOhms));
        return 20.0 * std::log10(std::abs(filter * gain * output));
    }

    /// @brief Twenty-four logarithmically spaced frequencies from 300 Hz to 2 kHz.
    /// @return The frequency list.
    std::vector<double> midband_frequencies()
    {
        std::vector<double> frequencies;
        for (std::size_t index = 0; index < 24; ++index)
        {
            frequencies.push_back(
                300.0 * std::pow(2000.0 / 300.0, static_cast<double>(index) / 23.0));
        }
        return frequencies;
    }
}

TEST(Unit_Dsp, DkTsToneBlockResponseMatchesItsClosedForm)
{
    // Closed form of the corrected network, in dB:
    //          100 Hz    1000 Hz   10000 Hz
    //   0.1   -1.3471    -6.2987   -22.5431
    //   0.5   -0.9946    -4.5852   -22.1373
    //   0.9   -0.6948    -2.2872   -19.7318
    // The old netlist read -30.4 dB at 1 kHz at noon and moved level, not corner, with the
    // knob; these numbers are the check that the correction is the one in the block.
    //
    // The 0.25 dB tolerance is the sweep measurement's own accuracy - D6a puts its worst error
    // above 20 Hz at 0.17 dB - not the block's. The block is linear, so any drive level
    // measures the same response; -40 dBFS is what measure::frequency_response uses.
    constexpr double          kRunRate = 384000.0;
    const std::vector<double> frequencies{ 20.0,   50.0,   100.0,  200.0,  500.0,
                                           1000.0, 2000.0, 5000.0, 10000.0 };

    for (const double tone : kSteppedTone)
    {
        ToneNode                 node;
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = tone;
        node.pot_positions[pots.level] = kLevelPotPosition;

        const auto measured = SushiDSP::measure::frequency_response(node, kRunRate, frequencies);
        ASSERT_TRUE(node.prepared());
        ASSERT_EQ(measured.size(), frequencies.size());
        for (const auto& point : measured)
        {
            EXPECT_NEAR(point.magnitude_db, tone_closed_form_db(point.frequency_hz, tone), 0.25)
                << tone << " at " << point.frequency_hz << " Hz";
        }
    }
}

TEST(Unit_Dsp, DkTsToneBlockCostsUnderOneDecibelAtOneHundredHertzAtNoon)
{
    // The acceptance the old reference could not have met. Its stage was an integrator with
    // unity gain at 1 / (2*pi*10k*220n) = 72.3 Hz, so it read -30.4 dB at 1 kHz at noon and
    // forced kLevelUnityGain to about 23.13 dB of make-up. R.G. Keen on the real stage: "there
    // is no net signal loss through the tone control stage, a gain of 1". What loss remains is
    // R7 against the 10 kOhm bias return, 20*log10(1 / (1 + 1k/10k)) = -0.828 dB, plus the
    // output network's -0.086 dB in its passband.
    EXPECT_NEAR(tone_closed_form_db(100.0, 0.5), -0.9946, 0.001);
    EXPECT_GT(tone_closed_form_db(100.0, 0.5), -1.0);
}

TEST(Unit_Dsp, DkTsToneBlockKnobSweepsSixteenDecibelsAtTheLegsTurnoverFrequency)
{
    // Keen puts the wiper leg's turnover where the 220 nF reaches 220 Ohm, "about 3.2KHz", and
    // that is where the knob has to do its work: at the + end the leg hangs on the passive
    // filter and cuts, at the - end it is the gain leg and boosts. The old netlist swung
    // 19.08 dB of flat level here while its corner moved 807 Hz to 760 Hz, which is a volume
    // control, not a tone control.
    const double dark = tone_closed_form_db(3200.0, 0.0 + 1.0e-9);
    const double bright = tone_closed_form_db(3200.0, 1.0 - 1.0e-9);
    EXPECT_NEAR(bright - dark, 16.185, 0.01);

    // At the bright end the gain leg wins over the passive filter for about an octave around
    // 1.1 kHz and the whole stage passes more than it is given: +0.435 dB at 1 kHz, peaking at
    // +0.465 dB at 1159 Hz. An integrator with 30 dB of insertion loss cannot do that, so this
    // is the single number that separates the corrected netlist from the one it replaced.
    EXPECT_GT(tone_closed_form_db(1000.0, 1.0 - 1.0e-9), 0.0);
}

TEST(Unit_Dsp, DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon)
{
    // The D5b/D6a tone control crossfaded a Butterworth low-pass against a Butterworth
    // high-pass, both at 720 Hz, so at noon the two cancelled and left a full null at 720 Hz.
    // The real network cannot notch: above the passive filter's 723 Hz corner it falls
    // monotonically at every setting. The closed form is checked at 1e-9 dB, which is what
    // pins the shape; the measured sweep is checked at 0.25 dB, its own accuracy, which is
    // what pins that this block is that shape. A null like D6a's is tens of dB deep and fails
    // the loose gate as readily as the tight one.
    constexpr double          kRunRate = 384000.0;
    const std::vector<double> frequencies = midband_frequencies();

    for (std::size_t index = 1; index < frequencies.size(); ++index)
    {
        EXPECT_LE(tone_closed_form_db(frequencies[index], 0.5),
                  tone_closed_form_db(frequencies[index - 1], 0.5) + 1.0e-9)
            << frequencies[index];
    }

    ToneNode                 node;
    const TonePotentiometers pots = build_tone_block(node.netlist);
    node.pot_positions[pots.tone] = 0.5;
    node.pot_positions[pots.level] = kLevelPotPosition;

    const auto measured = SushiDSP::measure::frequency_response(node, kRunRate, frequencies);
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
    // TONE is measured from the op-amp's + input, where the wiper leg only cuts, so TONE = 1 is
    // bright and the mapping is the identity. Before the 2026-09-06 correction this ran
    // backwards and stopped at 0.1 and 0.9, because the old netlist muted at TONE = 1.
    EXPECT_DOUBLE_EQ(tone_to_position(0.0), 0.0);
    EXPECT_DOUBLE_EQ(tone_to_position(0.5), 0.5);
    EXPECT_DOUBLE_EQ(tone_to_position(1.0), 1.0);
}

TEST(Unit_Dsp, DkTsToneBlockPreparesAtEveryTonePositionIncludingBothEndStops)
{
    // The old netlist muted at wiper position 1 - the pot shorted the wiper to the op-amp
    // output and the summing junction's equation collapsed - which is why tone_to_position used
    // to stop short of its own track. On the corrected network both ends are ordinary settings:
    // at 0 the wiper leg loads the passive filter, at 1 it is the whole gain leg, and neither
    // is degenerate. This is the case that fails first if someone reinstates the clamp.
    ToneNetlist              netlist;
    const TonePotentiometers pots = build_tone_block(netlist);
    EXPECT_EQ(netlist.node_count, 9u);

    ToneCircuit circuit;
    ASSERT_TRUE(circuit.prepare(netlist, 384000.0));
    for (const double position : { 0.0, 0.1, 0.5, 0.9, 1.0 })
    {
        circuit.set_potentiometer(pots.tone, position);
        circuit.set_potentiometer(pots.level, kLevelPotPosition);
        circuit.update_matrices_if_dirty();

        LinearPort            port;
        std::array<double, 1> outputs{};
        for (int sample = 0; sample < 64; ++sample)
        {
            circuit.process_sample(port, { 0.1 }, outputs);
            ASSERT_TRUE(std::isfinite(outputs[0])) << position;
        }
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}

TEST(Unit_Dsp, DkTsToneBlockMatchesTheLtspiceAcSweepWithinHalfADecibel)
{
    if (!kSpiceReferenceMatchesSchematic)
    {
        GTEST_SKIP() << "ts_tone_ac.csv was exported from the netlist the 2026-09-06 correction "
                     << "replaced. Regenerate it with: " << kRegenerateCommand
                     << ", commit the CSV, and set kSpiceReferenceMatchesSchematic to true.";
    }

    const std::string path = spice_path("ts_tone", "ac");
    const auto        reference = SushiDSP::measure::load_ac_csv(path);
    if (reference.frequency_hz.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path << ". Generate it with: "
                     << kRegenerateCommand << ", then commit the CSV.";
    }

    constexpr double          kRunRate = 192000.0;
    const std::vector<double> frequencies{ 20.0,   50.0,   100.0,  200.0,  500.0,
                                           1000.0, 2000.0, 5000.0, 10000.0 };

    for (std::size_t setting = 0; setting < kSteppedTone.size(); ++setting)
    {
        ToneNode                 node;
        const TonePotentiometers pots = build_tone_block(node.netlist);
        node.pot_positions[pots.tone] = kSteppedTone[setting];
        node.pot_positions[pots.level] = kLevelPotPosition;

        const auto measured = SushiDSP::measure::frequency_response(node, kRunRate, frequencies);
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
    if (!kSpiceReferenceMatchesSchematic)
    {
        GTEST_SKIP() << "ts_tone_tran.csv was exported from the netlist the 2026-09-06 "
                     << "correction replaced. Regenerate it with: " << kRegenerateCommand
                     << ", commit the CSV, and set kSpiceReferenceMatchesSchematic to true.";
    }

    const std::string path = spice_path("ts_tone", "tran");
    const auto        reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path << ". Generate it with: "
                     << kRegenerateCommand << ", then commit the CSV.";
    }

    ASSERT_GT(reference.sample_rate, 0.0);

    for (std::size_t setting = 0; setting < kSteppedTone.size(); ++setting)
    {
        ToneNetlist              netlist;
        const TonePotentiometers pots = build_tone_block(netlist);

        ToneCircuit circuit;
        LinearPort  port;
        ASSERT_TRUE(circuit.prepare(netlist, reference.sample_rate));
        circuit.set_potentiometer(pots.tone, kSteppedTone[setting]);
        circuit.set_potentiometer(pots.level, kLevelPotPosition);
        circuit.update_matrices_if_dirty();

        // tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point,
        // runs 40 ms and saves the last 20 ms. This block sits at zero at DC - the 4V5 rail is
        // an AC ground here - so a DK block starting from zero state has LTspice's initial
        // condition exactly, and the saved window is 20 whole cycles of the 1 kHz stimulus.
        // Replaying it ONCE therefore reproduces LTspice's own first 20 ms, and the second pass
        // is the comparison window. This is the clip block's procedure, for the same reason.
        //
        // Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before
        // comparison, so the band-limiting cannot flatter either one. The block is linear, so a
        // residual anywhere near the -60 dB gate means a component value or a node index is
        // wrong, not that a solver drifted.
        constexpr std::size_t kPreRollPasses = 1;

        std::vector<double> stimulus;
        std::vector<float>  reference_output;
        for (std::size_t row = 0; row < reference.input.size(); ++row)
        {
            if (reference.setting[row] == static_cast<int>(setting))
            {
                stimulus.push_back(static_cast<double>(reference.input[row]));
                reference_output.push_back(reference.output[row]);
            }
        }
        ASSERT_FALSE(reference_output.empty()) << "setting " << setting;

        std::vector<float>    circuit_output;
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

        band_limit(reference_output, reference.sample_rate, 10000.0);
        band_limit(circuit_output, reference.sample_rate, 10000.0);

        const double residual = SushiDSP::measure::null_test(reference_output, circuit_output, 256);
        std::printf("[ measure ] ts_tone tran setting %zu: %.2f dB residual\n", setting, residual);
        EXPECT_LE(residual, -60.0) << "setting " << setting;
    }
}
