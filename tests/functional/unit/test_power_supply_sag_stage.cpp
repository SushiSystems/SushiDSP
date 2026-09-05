/**************************************************************************/
/* test_power_supply_sag_stage.cpp                                        */
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
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/stages/PowerSupplySagStage.hpp"

TEST(Unit_Dsp, PowerSupplySagStageReducesGainUnderSustainedLoudInput)
{
    SushiDSP::PowerSupplySagStage stage(48000.0, 0.5f, 0.05f);

    float first_output = stage.process_sample(1.0f);

    float last_output = 0.0f;
    for (int i = 0; i < 5000; ++i)
    {
        last_output = stage.process_sample(1.0f);
    }

    EXPECT_LT(std::abs(last_output), std::abs(first_output));
}

TEST(Unit_Dsp, PowerSupplySagStageRecoversTowardUnityAfterSilence)
{
    SushiDSP::PowerSupplySagStage stage(48000.0, 0.5f, 0.05f);

    for (int i = 0; i < 5000; ++i)
    {
        stage.process_sample(1.0f);
    }
    const float sagged_output = stage.process_sample(1.0f);

    float recovered_output = 0.0f;
    for (int i = 0; i < 20000; ++i)
    {
        recovered_output = stage.process_sample(0.0f);
    }

    // With zero input the sag envelope should decay, so a subsequent unity-input sample should
    // be closer to unity than the deeply-sagged sample was.
    const float post_silence_output = stage.process_sample(1.0f);
    EXPECT_GT(std::abs(post_silence_output), std::abs(sagged_output));
    (void)recovered_output;
}

TEST(Unit_Dsp, PowerSupplySagStageNeverProducesNonFiniteOutput)
{
    SushiDSP::PowerSupplySagStage stage(48000.0, 0.9f, 0.01f);

    for (int i = 0; i < 2000; ++i)
    {
        const float output = stage.process_sample(10.0f);
        ASSERT_TRUE(std::isfinite(output));
    }
}

TEST(Unit_Dsp, PowerSupplySagStageResetClearsEnvelope)
{
    SushiDSP::PowerSupplySagStage stage_a(48000.0, 0.5f, 0.05f);
    SushiDSP::PowerSupplySagStage stage_b(48000.0, 0.5f, 0.05f);

    for (int i = 0; i < 1000; ++i)
    {
        stage_a.process_sample(1.0f);
    }
    stage_a.reset();

    const float from_reset = stage_a.process_sample(1.0f);
    const float from_fresh = stage_b.process_sample(1.0f);

    EXPECT_NEAR(from_reset, from_fresh, 1e-5f);
}
