/****************************************************************/
/* test_audio_taper_pot.cpp                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_audio_taper_pot.cpp
 * @brief The audio taper's endpoints, its law and its monotonicity.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/AudioTaperPot.hpp"

TEST(Unit_Dsp, AudioTaperPotSpansZeroToUnity)
{
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(1.0f), 1.0f);
}

TEST(Unit_Dsp, AudioTaperPotIsPositionToTheThreeHalves)
{
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.5f), 0.5f * std::sqrt(0.5f));
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.25f), 0.25f * std::sqrt(0.25f));
}

TEST(Unit_Dsp, AudioTaperPotRisesMonotonicallyAndSitsUnderALinearPot)
{
    float previous = -1.0f;
    for (int step = 0; step <= 100; ++step)
    {
        const float position = static_cast<float>(step) / 100.0f;
        const float passed = SushiDSP::audio_taper_pot(position);
        EXPECT_GT(passed, previous);
        EXPECT_LE(passed, position);
        previous = passed;
    }
}
