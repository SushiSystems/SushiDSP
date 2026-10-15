/**************************************************************************/
/* test_frame_delay.cpp                                                   */
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
 * @file test_frame_delay.cpp
 * @brief Tests that a frame delay holds its input back by the number of frames it was prepared with.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/frame_delay.hpp>

#include <array>

TEST(Small_FrameDelay, DelaysAnImpulseByItsFrameCount)
{
    SushiDSP::room::FrameDelay delay;
    delay.prepare(3u, 8u);
    delay.reset();
    std::array<float, 8> input{};
    input[0] = 1.0f;
    std::array<float, 8> output{};
    delay.process(input.data(), output.data(), 8u);
    EXPECT_FLOAT_EQ(output[0], 0.0f);
    EXPECT_FLOAT_EQ(output[2], 0.0f);
    EXPECT_FLOAT_EQ(output[3], 1.0f);
}

TEST(Small_FrameDelay, ResetForgetsThePreviousBlock)
{
    SushiDSP::room::FrameDelay delay;
    delay.prepare(3u, 8u);
    delay.reset();
    std::array<float, 8> loud{};
    loud.fill(1.0f);
    std::array<float, 8> output{};
    delay.process(loud.data(), output.data(), 8u);
    delay.reset();
    std::array<float, 8> silence{};
    delay.process(silence.data(), output.data(), 8u);
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}
