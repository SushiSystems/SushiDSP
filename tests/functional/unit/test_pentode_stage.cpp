/**************************************************************************/
/* test_pentode_stage.cpp                                                 */
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

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/AntiderivativeAntialiaser.hpp"
#include "SushiDSP/circuits/stages/PentodeStage.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    /// @brief The stage's settled output for a DC input.
    float settled(SushiDSP::PentodeStage& stage, float input, int samples = 100)
    {
        float output = 0.0f;
        for (int i = 0; i < samples; ++i)
        {
            output = stage.process_sample(input);
        }
        return output;
    }
}

namespace
{
    /// @brief A reference pentode for exercising the stage itself.
    ///
    /// The figures are the Philips EL34 datasheet's class B push-pull point, which is a real
    /// operating point rather than any product's tuning. A brick's test that reached into an
    /// amplifier module would fail whenever that amplifier was recalibrated.
    constexpr SushiDSP::PowerTubePreset kReferencePentode{
        33831.9f, 220000.0f, -38.0f, 6.882361f, SushiDSP::kPentodeEL34GridConduction,
        SushiDSP::PlateSwing{ 25.5f, 280.2777f } };
}

TEST(Unit_Dsp, PentodeStageIsDcStableForConstantInput)
{
    SushiDSP::PentodeStage stage(kReferencePentode);
    EXPECT_TRUE(std::isfinite(settled(stage, 1.0f, 1000)));
}

TEST(Unit_Dsp, PentodeStageSaturatesInsideItsPlateSwing)
{
    // The plate is bounded by the tube's own swing on each side: toward the supply by the idle
    // current's drop across the reflected load, toward the bottom by the rest of the datasheet's
    // full-signal swing. A grid driven far past cutoff or far into conduction reaches the tanh
    // ceiling and nothing else.
    SushiDSP::PentodeStage cut_off(kReferencePentode);
    SushiDSP::PentodeStage conducting(kReferencePentode);
    const float toward_supply = settled(cut_off, -400.0f);
    const float toward_bottom = settled(conducting, 400.0f);

    EXPECT_TRUE(std::isfinite(toward_supply));
    EXPECT_TRUE(std::isfinite(toward_bottom));
    EXPECT_GT(toward_supply, 0.0f);
    EXPECT_LT(toward_bottom, 0.0f);
    EXPECT_LE(toward_supply, kReferencePentode.plate_swing.headroom_to_supply_volts);
    EXPECT_GE(toward_bottom, -kReferencePentode.plate_swing.headroom_to_bottom_volts);
    EXPECT_GT(toward_supply, 0.99f * kReferencePentode.plate_swing.headroom_to_supply_volts);
    EXPECT_LT(toward_bottom, -0.99f * kReferencePentode.plate_swing.headroom_to_bottom_volts);
}

TEST(Unit_Dsp, PentodeStageIsNearlyLinearForSmallInput)
{
    // Inside the window the grid draws no current, the 220 kOhm leak against the 33.8 kOhm
    // source sets the grid at 0.867 of the input, and the plate reads that times the gain.
    SushiDSP::PentodeStage stage(kReferencePentode);
    const float output = settled(stage, 0.1f, 50);
    const float grid_divider = kReferencePentode.grid_leak_resistance_ohms
        / (kReferencePentode.grid_leak_resistance_ohms + kReferencePentode.source_resistance_ohms);
    const float linear_prediction = -kReferencePentode.gain * grid_divider * 0.1f;
    EXPECT_NEAR(output, linear_prediction, std::abs(linear_prediction) * 0.02f);
}

TEST(Unit_Dsp, PentodeStageGridConductsAtTheCathodeNotAtTheBiasPoint)
{
    // The grid law is referenced to the cathode: the whole |Vk| window is clean, and the grid
    // only draws current once the input has carried it past 0 V. A +30 V swing on a -38 V grid
    // comes back as the linear plate answer through the tanh, to the float; the same swing on a
    // -20 V grid has crossed its window, the 150 kOhm conducting slope loads the 33.8 kOhm
    // source, and the plate is held 4.28 V short of that answer (measured 2026-09-07).
    SushiDSP::PentodeStage inside(kReferencePentode);
    SushiDSP::PentodeStage crossed(
        kReferencePentode.source_resistance_ohms, kReferencePentode.grid_leak_resistance_ohms, -20.0f,
        kReferencePentode.gain, kReferencePentode.grid_conduction, kReferencePentode.plate_swing);
    const float grid_divider = kReferencePentode.grid_leak_resistance_ohms
        / (kReferencePentode.grid_leak_resistance_ohms + kReferencePentode.source_resistance_ohms);
    const float linear = -kReferencePentode.gain * grid_divider * 30.0f;
    const float through_tanh = kReferencePentode.plate_swing.bound(linear);

    EXPECT_NEAR(settled(inside, 30.0f), through_tanh, 0.01f * std::abs(through_tanh));
    EXPECT_GT(settled(crossed, 30.0f), through_tanh + 2.0f);
}

TEST(Unit_Dsp, PentodeStageResetClearsWarmStartState)
{
    SushiDSP::PentodeStage stage_a(kReferencePentode);
    SushiDSP::PentodeStage stage_b(kReferencePentode);

    stage_a.process_sample(100.0f);
    stage_a.reset();

    EXPECT_NEAR(stage_a.process_sample(0.5f), stage_b.process_sample(0.5f), 1e-4f);
}

TEST(Unit_Dsp, PentodePairIsOddSymmetricInsideTheWindowAndABiasDifferenceMovesTheHandover)
{
    // The push-pull composite el34_a(u) - el34_b(-u) is odd in u while both grids stay inside
    // their windows: the two tubes are the same function of their input. Since the grid law is
    // referenced to the cathode, two halves with different bias voltages have different windows
    // and the composite acquires an even part once the drive reaches the nearer window; a
    // cathode-bias difference is a real handover asymmetry, where the bias-referenced stage of
    // DA4 cancelled it exactly. Measured 2026-09-07: an even part of exactly 0 at equal bias, and
    // 3.411 V for -38 V against -25 V at 60 V of drive, on an odd reference of 183.5 V.
    auto composite = [](float bias_a, float bias_b, float u)
    {
        SushiDSP::PentodeStage a(
            kReferencePentode.source_resistance_ohms, kReferencePentode.grid_leak_resistance_ohms, bias_a,
            kReferencePentode.gain, kReferencePentode.grid_conduction, kReferencePentode.plate_swing);
        SushiDSP::PentodeStage b(
            kReferencePentode.source_resistance_ohms, kReferencePentode.grid_leak_resistance_ohms, bias_b,
            kReferencePentode.gain, kReferencePentode.grid_conduction, kReferencePentode.plate_swing);
        return settled(a, u, 64) - settled(b, -u, 64);
    };
    auto peak_even_part = [&](float bias_a, float bias_b, float drive)
    {
        float peak = 0.0f;
        for (int step = 1; step <= 20; ++step)
        {
            const float u = drive * static_cast<float>(step) / 20.0f;
            peak = std::max(peak, std::abs(0.5f * (composite(bias_a, bias_b, u) + composite(bias_a, bias_b, -u))));
        }
        return peak;
    };

    const float odd_reference = std::abs(composite(-38.0f, -38.0f, 30.0f));
    ASSERT_GT(odd_reference, 100.0f);
    EXPECT_LT(peak_even_part(-38.0f, -38.0f, 30.0f), 1e-3f * odd_reference);
    EXPECT_GT(peak_even_part(-38.0f, -25.0f, 60.0f), 0.01f * odd_reference);
}

TEST(Unit_Dsp, PentodeStagePresetConstructorMatchesTheSixArgumentOne)
{
    SushiDSP::PentodeStage from_preset(kReferencePentode);
    SushiDSP::PentodeStage from_fields(
        kReferencePentode.source_resistance_ohms, kReferencePentode.grid_leak_resistance_ohms,
        kReferencePentode.cathode_bias_volts, kReferencePentode.gain, kReferencePentode.grid_conduction,
        kReferencePentode.plate_swing);
    for (int i = 0; i < 2000; ++i)
    {
        const float x = 40.0f * static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / 48000.0));
        EXPECT_FLOAT_EQ(from_preset.process_sample(x), from_fields.process_sample(x));
    }
}

TEST(Unit_Dsp, PentodeStageAntialiaserAntiderivativeDifferentiatesBackToTheTransfer)
{
    // The EL34's knee sits 38.5 V above rest and its plate reaches 280 V toward the bottom, so
    // the identity the triode's antialiaser keeps, dG/du = bound(-gain u) dg/du, is checked on
    // the power tube's own table.
    const SushiDSP::PowerTubePreset& preset = kReferencePentode;
    const SushiDSP::AntiderivativeAntialiaser antialiaser(
        preset.source_resistance_ohms, preset.grid_leak_resistance_ohms, preset.cathode_bias_volts, preset.gain,
        preset.grid_conduction, preset.plate_swing);
    const double knee = preset.grid_conduction.threshold_volts - static_cast<double>(preset.cathode_bias_volts);
    const double transition = preset.grid_conduction.transition_volts;
    constexpr double kDelta = 1e-4;
    for (double excursion = knee - 40.0 * transition; excursion <= knee + 40.0 * transition; excursion += transition / 37.0)
    {
        const double numeric_slope
            = (antialiaser.antiderivative(excursion + kDelta) - antialiaser.antiderivative(excursion - kDelta)) / (2.0 * kDelta);
        const double input_slope
            = (antialiaser.input_for_excursion(excursion + kDelta) - antialiaser.input_for_excursion(excursion - kDelta)) / (2.0 * kDelta);
        const double plate = preset.plate_swing.bound(-static_cast<double>(preset.gain) * excursion);
        EXPECT_NEAR(numeric_slope, plate * input_slope, 1e-7 * preset.plate_swing.headroom_to_bottom_volts * input_slope)
            << "excursion " << excursion;
    }
}
