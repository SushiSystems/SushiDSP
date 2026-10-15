/**************************************************************************/
/* test_take.cpp                                                          */
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
 * @file test_take.cpp
 * @brief Tests that a take holds the shape it was made with and returns the samples written to it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/take.hpp>

TEST(Take, KeepsItsChannelsFramesAndRate)
{
    const SushiDSP::Take take(2u, 48000.0, 128u);
    EXPECT_EQ(take.channels(), 2u);
    EXPECT_EQ(take.frames(), 128u);
    EXPECT_EQ(take.sample_rate(), 48000.0);
}

TEST(Take, ReturnsTheSamplesWrittenToAChannel)
{
    SushiDSP::Take take(2u, 48000.0, 128u);
    take.mutable_channel(1)[5] = 0.25f;
    ASSERT_NE(take.channel(1), nullptr);
    EXPECT_FLOAT_EQ(take.channel(1)[5], 0.25f);
}
