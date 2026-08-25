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

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/jcm800/PentodeStage.hpp"

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
