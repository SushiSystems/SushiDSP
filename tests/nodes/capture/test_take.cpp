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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_take.cpp
 * @brief Tests that a take holds the shape it was made with and returns the samples written to it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/take.hpp>

TEST(Small_Take, KeepsItsChannelsFramesAndRate)
{
    const SushiDSP::Take take(2u, 48000.0, 128u);
    EXPECT_EQ(take.channels(), 2u);
    EXPECT_EQ(take.frames(), 128u);
    EXPECT_EQ(take.sample_rate(), 48000.0);
}

TEST(Small_Take, ReturnsTheSamplesWrittenToAChannel)
{
    SushiDSP::Take take(2u, 48000.0, 128u);
    take.mutable_channel(1)[5] = 0.25f;
    ASSERT_NE(take.channel(1), nullptr);
    EXPECT_EQ(take.channel(1)[5], 0.25f);
}

TEST(Small_Take, HasNoChannelPastItsLast)
{
    SushiDSP::Take take(2u, 96000.0, 512u);

    EXPECT_EQ(take.channel(2), nullptr);
    EXPECT_EQ(take.mutable_channel(2), nullptr);
}

TEST(Small_Take, KeepsAWrittenSampleInItsOwnChannel)
{
    SushiDSP::Take take(2u, 96000.0, 512u);

    take.mutable_channel(1)[7] = 0.75f;

    EXPECT_EQ(take.channel(1)[7], 0.75f);
    EXPECT_EQ(take.channel(0)[7], 0.0f);
}
