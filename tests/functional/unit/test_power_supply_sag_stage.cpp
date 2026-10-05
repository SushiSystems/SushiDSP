/****************************************************************/
/* test_power_supply_sag_stage.cpp                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_power_supply_sag_stage.cpp
 * @brief Tests the sag and recovery of PowerSupplySagStage.
 * @author Mustafa Garip
 */

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

    // The sag envelope decays over silence. See tests/functional/unit/README.md.
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
