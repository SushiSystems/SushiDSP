/**************************************************************************/
/* test_audio_taper_pot.cpp                                               */
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
 * @file test_audio_taper_pot.cpp
 * @brief The audio taper's endpoints, its law and its monotonicity.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/audio_taper_pot.hpp"

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
