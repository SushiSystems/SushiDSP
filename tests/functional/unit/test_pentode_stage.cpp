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

#include "SushiDSP/circuits/stages/PentodeStage.hpp"

namespace
{
    constexpr float kSourceResistance = 10000.0f;
    constexpr float kGridLeakResistance = 220000.0f;
    constexpr float kCathodeBias = -20.0f;   // typical EL34 fixed/cathode bias magnitude
    constexpr float kGain = 8.0f;
    constexpr float kSaturationCurrent = 1e-9f;
    constexpr float kThermalVoltage = 0.02585f;
    constexpr float kPlateSaturationDrive = 0.05f;
}

TEST(Unit_Dsp, PentodeStageIsDcStableForConstantInput)
{
    SushiDSP::PentodeStage stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
        kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);

    float output = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        output = stage.process_sample(1.0f);
    }

    EXPECT_TRUE(std::isfinite(output));
}

TEST(Unit_Dsp, PentodeStageSaturatesAndStaysBounded)
{
    SushiDSP::PentodeStage stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
        kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);

    float output = 0.0f;
    for (int i = 0; i < 200; ++i)
    {
        output = stage.process_sample(30.0f);
    }

    const float saturation_ceiling = 1.0f / kPlateSaturationDrive;
    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LT(std::abs(output), saturation_ceiling + 1.0f);
}

TEST(Unit_Dsp, PentodeStageIsNearlyLinearForSmallInput)
{
    SushiDSP::PentodeStage stage(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
        kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);

    float output = 0.0f;
    for (int i = 0; i < 50; ++i)
    {
        output = stage.process_sample(0.001f);
    }

    const float linear_prediction = -kGain * 0.001f;
    EXPECT_NEAR(output, linear_prediction, std::abs(linear_prediction) * 0.2f + 1e-4f);
}

TEST(Unit_Dsp, PentodeStageResetClearsWarmStartState)
{
    SushiDSP::PentodeStage stage_a(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
        kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);
    SushiDSP::PentodeStage stage_b(
        kSourceResistance, kGridLeakResistance, kCathodeBias, kGain,
        kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);

    stage_a.process_sample(10.0f);
    stage_a.reset();

    const float from_reset = stage_a.process_sample(0.5f);
    const float from_fresh = stage_b.process_sample(0.5f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-4f);
}

TEST(Unit_Dsp, PentodePairAsymmetryNeedsAnInputReferredOffsetNotACathodeBiasDifference)
{
    // The push-pull pair's composite is el34_a(u) - el34_b(-u). This pins the ruling behind
    // Jcm800's kPowerTubeGridBiasMismatchVolts: PentodeStage's node equation is bias-invariant,
    // so giving the two halves different cathode-bias constants buys exactly nothing:
    // substituting Vg - Vk removes Vk from the source term, the grid leak, the solver bounds, the warm start
    // and the plate output alike, leaving the composite exactly odd in u. Only an offset applied
    // to the input, outside the stage's Vk-relative coordinate system, produces a real handover
    // asymmetry.
    auto composite = [](float bias_a, float bias_b, float input_offset, float u)
    {
        SushiDSP::PentodeStage a(
            kSourceResistance, kGridLeakResistance, bias_a, kGain,
            kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);
        SushiDSP::PentodeStage b(
            kSourceResistance, kGridLeakResistance, bias_b, kGain,
            kSaturationCurrent, kThermalVoltage, kPlateSaturationDrive);

        float a_out = 0.0f;
        float b_out = 0.0f;
        for (int i = 0; i < 64; ++i)
        {
            a_out = a.process_sample(u);
            b_out = b.process_sample(-u - input_offset);
        }
        return a_out - b_out;
    };

    // The even part of the composite over the +/-0.38 V drive range the phase inverter actually
    // delivers. Zero even part means zero even harmonics, i.e. no crossover character at all.
    auto peak_even_part = [&](float bias_a, float bias_b, float input_offset)
    {
        float peak = 0.0f;
        for (int step = 1; step <= 20; ++step)
        {
            const float u = 0.019f * step;
            const float even = 0.5f * (composite(bias_a, bias_b, input_offset, u)
                                     + composite(bias_a, bias_b, input_offset, -u));
            peak = std::max(peak, std::abs(even));
        }
        return peak;
    };

    const float odd_reference = std::abs(composite(-20.0f, -20.0f, 0.0f, 0.38f));
    ASSERT_GT(odd_reference, 0.1f);

    // Measured: the cathode-bias difference gives an even part of exactly 0.0, the input-referred
    // offset gives 0.229 against an odd reference of 4.83 (4.7%).
    EXPECT_LT(peak_even_part(-20.0f, -20.5f, 0.0f), 1e-6f * odd_reference);
    EXPECT_GT(peak_even_part(-20.0f, -20.0f, 0.03f), 0.01f * odd_reference);
}
