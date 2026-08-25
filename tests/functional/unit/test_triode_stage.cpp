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

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/TriodeStage.hpp"

namespace
{
    constexpr float kSourceResistance = 10000.0f;
    constexpr float kGridLeakResistance = 1000000.0f;   // 1M, typical 12AX7 grid-leak
    constexpr float kCathodeBias = -1.5f;                // typical 12AX7 self-bias point
    constexpr float kGain = 60.0f;                       // approximate 12AX7 mu-derived stage gain
    constexpr float kSaturationCurrent = 1e-9f;
    constexpr float kThermalVoltage = 0.02585f;
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
