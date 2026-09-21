/**************************************************************************/
/* test_audio_taper_pot.cpp                                               */
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
