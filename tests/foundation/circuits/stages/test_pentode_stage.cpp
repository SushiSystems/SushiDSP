/**************************************************************************/
/* test_pentode_stage.cpp                                                 */
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
 * @file test_pentode_stage.cpp
 * @brief Tests the saturation, grid conduction and antialiasing of PentodeStage.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp>
#include <SushiDSP/foundation/circuits/stages/pentode_stage.hpp>

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

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
} // namespace

namespace
{
    /// @brief A reference pentode for exercising the stage itself.
    ///
    /// The figures are the Philips EL34 datasheet's class B push-pull point, which is a real
    /// operating point rather than any product's tuning. A brick's test that reached into an
    /// amplifier module would fail whenever that amplifier was recalibrated.
    constexpr SushiDSP::PowerTubePreset K_REFERENCE_PENTODE{
        33831.9f,
        220000.0f,
        -38.0f,
        6.882361f,
        SushiDSP::K_PENTODE_EL34_GRID_CONDUCTION,
        SushiDSP::PlateSwing{25.5f, 280.2777f}};
} // namespace

TEST(Unit_Dsp, PentodeStageIsDcStableForConstantInput)
{
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    EXPECT_TRUE(std::isfinite(settled(stage, 1.0f, 1000)));
}

TEST(Unit_Dsp, PentodeStageSaturatesInsideItsPlateSwing)
{
    // The plate is bounded by the tube's swing on each side. See tests/foundation/circuits/stages/README.md.
    SushiDSP::PentodeStage cut_off(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage conducting(K_REFERENCE_PENTODE);
    const float toward_supply = settled(cut_off, -400.0f);
    const float toward_bottom = settled(conducting, 400.0f);

    EXPECT_TRUE(std::isfinite(toward_supply));
    EXPECT_TRUE(std::isfinite(toward_bottom));
    EXPECT_GT(toward_supply, 0.0f);
    EXPECT_LT(toward_bottom, 0.0f);
    EXPECT_LE(toward_supply, K_REFERENCE_PENTODE.plate_swing.headroom_to_supply_volts);
    EXPECT_GE(toward_bottom, -K_REFERENCE_PENTODE.plate_swing.headroom_to_bottom_volts);
    EXPECT_GT(toward_supply, 0.99f * K_REFERENCE_PENTODE.plate_swing.headroom_to_supply_volts);
    EXPECT_LT(toward_bottom, -0.99f * K_REFERENCE_PENTODE.plate_swing.headroom_to_bottom_volts);
}

TEST(Unit_Dsp, PentodeStageIsNearlyLinearForSmallInput)
{
    // The grid sits at 0.867 of the input inside the window. See tests/foundation/circuits/stages/README.md.
    SushiDSP::PentodeStage stage(K_REFERENCE_PENTODE);
    const float output = settled(stage, 0.1f, 50);
    const float grid_divider = K_REFERENCE_PENTODE.grid_leak_resistance_ohms /
                               (K_REFERENCE_PENTODE.grid_leak_resistance_ohms +
                                K_REFERENCE_PENTODE.source_resistance_ohms);
    const float linear_prediction = -K_REFERENCE_PENTODE.gain * grid_divider * 0.1f;
    EXPECT_NEAR(output, linear_prediction, std::abs(linear_prediction) * 0.02f);
}

TEST(Unit_Dsp, PentodeStageGridConductsAtTheCathodeNotAtTheBiasPoint)
{
    // The grid law is referenced to the cathode. See tests/foundation/circuits/stages/README.md.
    SushiDSP::PentodeStage inside(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage crossed(K_REFERENCE_PENTODE.source_resistance_ohms,
                                   K_REFERENCE_PENTODE.grid_leak_resistance_ohms, -20.0f,
                                   K_REFERENCE_PENTODE.gain, K_REFERENCE_PENTODE.grid_conduction,
                                   K_REFERENCE_PENTODE.plate_swing);
    const float grid_divider = K_REFERENCE_PENTODE.grid_leak_resistance_ohms /
                               (K_REFERENCE_PENTODE.grid_leak_resistance_ohms +
                                K_REFERENCE_PENTODE.source_resistance_ohms);
    const float linear = -K_REFERENCE_PENTODE.gain * grid_divider * 30.0f;
    const float through_tanh = K_REFERENCE_PENTODE.plate_swing.bound(linear);

    EXPECT_NEAR(settled(inside, 30.0f), through_tanh, 0.01f * std::abs(through_tanh));
    EXPECT_GT(settled(crossed, 30.0f), through_tanh + 2.0f);
}

TEST(Unit_Dsp, PentodeStageResetClearsWarmStartState)
{
    SushiDSP::PentodeStage stage_a(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage stage_b(K_REFERENCE_PENTODE);

    stage_a.process_sample(100.0f);
    stage_a.reset();

    EXPECT_NEAR(stage_a.process_sample(0.5f), stage_b.process_sample(0.5f), 1e-4f);
}

TEST(Unit_Dsp, PentodePairIsOddSymmetricInsideTheWindowAndABiasDifferenceMovesTheHandover)
{
    // The composite is odd while both grids stay in window. See tests/foundation/circuits/stages/README.md.
    auto composite = [](float bias_a, float bias_b, float u)
    {
        SushiDSP::PentodeStage a(K_REFERENCE_PENTODE.source_resistance_ohms,
                                 K_REFERENCE_PENTODE.grid_leak_resistance_ohms, bias_a,
                                 K_REFERENCE_PENTODE.gain, K_REFERENCE_PENTODE.grid_conduction,
                                 K_REFERENCE_PENTODE.plate_swing);
        SushiDSP::PentodeStage b(K_REFERENCE_PENTODE.source_resistance_ohms,
                                 K_REFERENCE_PENTODE.grid_leak_resistance_ohms, bias_b,
                                 K_REFERENCE_PENTODE.gain, K_REFERENCE_PENTODE.grid_conduction,
                                 K_REFERENCE_PENTODE.plate_swing);
        return settled(a, u, 64) - settled(b, -u, 64);
    };
    auto peak_even_part = [&](float bias_a, float bias_b, float drive)
    {
        float peak = 0.0f;
        for (int step = 1; step <= 20; ++step)
        {
            const float u = drive * static_cast<float>(step) / 20.0f;
            peak = std::max(peak, std::abs(0.5f * (composite(bias_a, bias_b, u) +
                                                   composite(bias_a, bias_b, -u))));
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
    SushiDSP::PentodeStage from_preset(K_REFERENCE_PENTODE);
    SushiDSP::PentodeStage from_fields(
        K_REFERENCE_PENTODE.source_resistance_ohms, K_REFERENCE_PENTODE.grid_leak_resistance_ohms,
        K_REFERENCE_PENTODE.cathode_bias_volts, K_REFERENCE_PENTODE.gain,
        K_REFERENCE_PENTODE.grid_conduction, K_REFERENCE_PENTODE.plate_swing);
    for (int i = 0; i < 2000; ++i)
    {
        const float x = 40.0f * static_cast<float>(std::sin(2.0 * K_PI * 440.0 * i / 48000.0));
        EXPECT_FLOAT_EQ(from_preset.process_sample(x), from_fields.process_sample(x));
    }
}

TEST(Unit_Dsp, PentodeStageAntialiaserAntiderivativeDifferentiatesBackToTheTransfer)
{
    // dG/du = bound(-gain u) dg/du on the EL34's table. See tests/foundation/circuits/stages/README.md.
    const SushiDSP::PowerTubePreset& preset = K_REFERENCE_PENTODE;
    const SushiDSP::AntiderivativeAntialiaser antialiaser(
        preset.source_resistance_ohms, preset.grid_leak_resistance_ohms, preset.cathode_bias_volts,
        preset.gain, preset.grid_conduction, preset.plate_swing);
    const double knee =
        preset.grid_conduction.threshold_volts - static_cast<double>(preset.cathode_bias_volts);
    const double transition = preset.grid_conduction.transition_volts;
    constexpr double K_DELTA = 1e-4;
    for (double excursion = knee - 40.0 * transition; excursion <= knee + 40.0 * transition;
         excursion += transition / 37.0)
    {
        const double numeric_slope = (antialiaser.antiderivative(excursion + K_DELTA) -
                                      antialiaser.antiderivative(excursion - K_DELTA)) /
                                     (2.0 * K_DELTA);
        const double input_slope = (antialiaser.input_for_excursion(excursion + K_DELTA) -
                                    antialiaser.input_for_excursion(excursion - K_DELTA)) /
                                   (2.0 * K_DELTA);
        const double plate =
            preset.plate_swing.bound(-static_cast<double>(preset.gain) * excursion);
        EXPECT_NEAR(numeric_slope, plate * input_slope,
                    1e-7 * preset.plate_swing.headroom_to_bottom_volts * input_slope)
            << "excursion " << excursion;
    }
}
