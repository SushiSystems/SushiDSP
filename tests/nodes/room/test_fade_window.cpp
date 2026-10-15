/**************************************************************************/
/* test_fade_window.cpp                                                   */
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
 * @file test_fade_window.cpp
 * @brief Tests that the fade weight rises monotonically to one over the crossfade window.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fade_window.hpp>

#include <cstdint>

TEST(Small_FadeWindow, CrossfadeIsTwoEarsOfFiveHundredTwelveFrames)
{
    EXPECT_EQ(SushiDSP::room::K_CROSSFADE_FRAMES, 512u);
    EXPECT_EQ(SushiDSP::room::K_EARS, 2u);
}

TEST(Small_FadeWindow, WeightEndsAtOne)
{
    EXPECT_FLOAT_EQ(SushiDSP::room::fade_weight(511u, 512u), 1.0f);
}

TEST(Small_FadeWindow, WeightRisesMonotonicallyAndStaysInUnitRange)
{
    float previous = 0.0f;
    for (std::uint32_t frame = 0; frame < 512u; ++frame)
    {
        const float weight = SushiDSP::room::fade_weight(frame, 512u);
        EXPECT_GT(weight, 0.0f);
        EXPECT_LE(weight, 1.0f);
        EXPECT_GE(weight, previous);
        previous = weight;
    }
}
