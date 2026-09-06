/**************************************************************************/
/* test_triode_stage.cpp                                                  */
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

#include "SushiDSP/circuits/stages/TriodeStage.hpp"

namespace
{
    constexpr float kSourceResistance = 10000.0f;
    constexpr float kGridLeakResistance = 1000000.0f;   // 1M, typical 12AX7 grid-leak
    constexpr float kCathodeBias = -1.5f;                // typical 12AX7 self-bias point
    constexpr float kGain = 60.0f;                       // approximate 12AX7 mu-derived stage gain
    constexpr float kSaturationCurrent = 1e-9f;
    constexpr float kThermalVoltage = 0.02585f;
    constexpr double kPi = 3.14159265358979323846;
}

TEST(Unit_Dsp, TriodeStageIsDcStableForConstantInput)
{
    SushiDSP::TriodeStage stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = stage.process_sample(0.1f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, TriodeStageIsInvertingForSmallSignal)
{
    SushiDSP::TriodeStage stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        output = stage.process_sample(0.05f);
    }

    EXPECT_LT(output, 0.0f);
}

TEST(Unit_Dsp, TriodeStageClipsAsymmetrically)
{
    // Grid conduction only engages on the positive-going half (Vg > Vk); a large enough swing in
    // both directions should produce a measurably larger deviation from linear gain on one side.
    SushiDSP::TriodeStage positive_stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TriodeStage negative_stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);

    constexpr float kSwing = 1.5f;  // large enough to push Vg well past Vk on the positive stage
    float positive_output = 0.0f;
    float negative_output = 0.0f;
    for (int i = 0; i < 100; ++i)
    {
        positive_output = positive_stage.process_sample(kSwing);
        negative_output = negative_stage.process_sample(-kSwing);
    }

    const float linear_prediction_positive = -kGain * kSwing;
    const float linear_prediction_negative = -kGain * -kSwing;

    const float positive_deviation = std::abs(positive_output - linear_prediction_positive);
    const float negative_deviation = std::abs(negative_output - linear_prediction_negative);

    EXPECT_GT(positive_deviation, negative_deviation * 2.0f);
}

TEST(Unit_Dsp, TriodeStageResetReturnsToCathodeBiasWarmStart)
{
    SushiDSP::TriodeStage stage_a(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TriodeStage stage_b(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain, kSaturationCurrent, kThermalVoltage);

    stage_a.process_sample(1.0f);
    stage_a.reset();

    const float from_reset = stage_a.process_sample(0.01f);
    const float from_fresh = stage_b.process_sample(0.01f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-4f);
}

TEST(Unit_Dsp, TriodeStageResolvesPastTheOldSymmetricCutoffBound)
{
    // Guards Task 9's asymmetric Newton-Raphson bound directly at the stage level, where it's
    // actually load-bearing (the composite Jcm800 node's own output is too many stages and too
    // much downstream saturation removed from a single grid's excursion to discriminate this
    // reliably - measured attempts at the node level moved by single-digit percent either way,
    // regardless of which bound was compiled in). See the Task 9 report's ruling.
    //
    // Same component values Jcm800.cpp uses for v1b_ - the stage whose grid the report measures
    // reaching 1.56-2.81 V, past the old +/-60*Vt (1.551 V) bound. A -3.5 V grid swing is deep
    // in the negative/cutoff half's affine region (no exponential term), where Newton's answer is
    // just the unclamped affine root: under the old symmetric bound that root is unreachable and
    // the stage pins at a fixed plate voltage regardless of how much further the grid is driven;
    // under Task 9's widened -160*Vt (4.136 V) cutoff bound it is not, and the plate output tracks
    // the drive instead.
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f);

    stage.reset();
    const float plate_at_moderate_swing = stage.process_sample(-2.5f);
    stage.reset();
    const float plate_at_deep_swing = stage.process_sample(-3.5f);

    // Under the old +/-60*Vt bound both readings pin at the same clamped plate voltage (93.06,
    // independently reproduced by reverting the bound and rerunning this exact scenario). Under
    // the widened bound the deeper swing must resolve further, not pin at the moderate swing's
    // value.
    EXPECT_GT(plate_at_deep_swing, plate_at_moderate_swing * 1.3f);
}

TEST(Unit_Dsp, TriodeStageSixArgumentPathIsUnchangedByThePlateSwingOverload)
{
    SushiDSP::TriodeStage before(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f);
    SushiDSP::TriodeStage huge(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 1e9f, 1e9f });
    for (int i = 0; i < 4000; ++i)
    {
        const float x = 3.0f * static_cast<float>(std::sin(2.0 * kPi * 440.0 * i / 48000.0));
        const float a = before.process_sample(x), b = huge.process_sample(x);
        EXPECT_NEAR(a, b, 1e-3f * std::max(1.0f, std::fabs(a)));
    }
}

TEST(Unit_Dsp, TriodeStagePlateSwingBoundsBothSidesAsymmetrically)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    float positive_peak = 0.0f, negative_peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = stage.process_sample(10.0f * static_cast<float>(std::sin(2.0 * kPi * 100.0 * i / 48000.0)));
        positive_peak = std::max(positive_peak, y);
        negative_peak = std::min(negative_peak, y);
    }
    // The cutoff side runs into the ceiling: the solver's 160 * Vt grid clamp puts at most
    // 248 V into the tanh, which 194 V of supply headroom folds back to about 166 V.
    EXPECT_LT(positive_peak, 194.0f);
    EXPECT_GT(positive_peak, 0.8f * 194.0f);

    // The conduction side stops well short of its own bound, because grid conduction limits the
    // grid's rise long before the plate reaches the load-line bottom.
    EXPECT_GT(negative_peak, -115.0f);
    EXPECT_LT(negative_peak, -10.0f);

    EXPECT_GT(positive_peak, 3.0f * -negative_peak);
}

TEST(Unit_Dsp, TriodeStageSmallSignalGainIsUnchangedByThePlateSwing)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        peak = std::max(peak, std::fabs(stage.process_sample(0.01f * static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / 48000.0)))));
    }
    EXPECT_NEAR(peak, 0.6f, 0.006f);
}

TEST(Unit_Dsp, TriodeStageReportsThePeakGridExcursionSinceReset)
{
    SushiDSP::TriodeStage stage(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f, SushiDSP::PlateSwing{ 194.0f, 115.0f });
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
    for (int i = 0; i < 480; ++i) stage.process_sample(-2.0f);
    EXPECT_NEAR(stage.peak_grid_excursion_volts(), 2.0f, 0.05f);
    stage.reset();
    EXPECT_FLOAT_EQ(stage.peak_grid_excursion_volts(), 0.0f);
}
