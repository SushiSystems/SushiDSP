/**************************************************************************/
/* test_tube_screamer_clipping_stage.cpp                                  */
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

#include "SushiDSP/tubescreamer/TubeScreamerClippingStage.hpp"

namespace
{
    constexpr float kInputResistance = 4700.0f;      // R1, TS808-typical
    constexpr float kFeedbackResistance = 51000.0f;  // Rf at a mid-drive setting
    constexpr float kSaturationCurrent = 1e-12f;
    constexpr float kThermalVoltage = 0.02585f;
}

TEST(Unit_Dsp, TubeScreamerClippingStageIsDcStableForConstantInput)
{
    SushiDSP::TubeScreamerClippingStage stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = stage.process_sample(0.05f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, TubeScreamerClippingStageIsInvertingAndApproximatelyOddSymmetric)
{
    SushiDSP::TubeScreamerClippingStage positive_stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage negative_stage(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    float positive_output = 0.0f;
    float negative_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        positive_output = positive_stage.process_sample(0.02f);
        negative_output = negative_stage.process_sample(-0.02f);
    }

    EXPECT_LT(positive_output, 0.0f);  // inverting: positive in -> negative out
    EXPECT_NEAR(positive_output, -negative_output, 1e-3f);
}

TEST(Unit_Dsp, TubeScreamerClippingStageHigherFeedbackResistanceGivesMoreGainBeforeClipping)
{
    SushiDSP::TubeScreamerClippingStage low_feedback(
        kInputResistance, 1000.0f, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage high_feedback(
        kInputResistance, 200000.0f, kSaturationCurrent, kThermalVoltage);

    float low_output = 0.0f;
    float high_output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        low_output = low_feedback.process_sample(0.01f);
        high_output = high_feedback.process_sample(0.01f);
    }

    EXPECT_LT(std::abs(low_output), std::abs(high_output));
}

TEST(Unit_Dsp, TubeScreamerClippingStageSetFeedbackResistanceChangesBehaviorLive)
{
    SushiDSP::TubeScreamerClippingStage stage(
        kInputResistance, 1000.0f, kSaturationCurrent, kThermalVoltage);

    float before = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        before = stage.process_sample(0.01f);
    }

    stage.set_feedback_resistance(200000.0f);

    float after = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        after = stage.process_sample(0.01f);
    }

    EXPECT_LT(std::abs(before), std::abs(after));
}

TEST(Unit_Dsp, TubeScreamerClippingStageResetClearsWarmStartState)
{
    SushiDSP::TubeScreamerClippingStage stage_a(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);
    SushiDSP::TubeScreamerClippingStage stage_b(
        kInputResistance, kFeedbackResistance, kSaturationCurrent, kThermalVoltage);

    stage_a.process_sample(0.1f);
    stage_a.reset();

    const float from_reset = stage_a.process_sample(0.005f);
    const float from_fresh = stage_b.process_sample(0.005f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
