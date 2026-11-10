/**************************************************************************/
/* test_in_memory_block_clock.cpp                                         */
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
 * @file test_in_memory_block_clock.cpp
 * @brief Tests that the in-memory BlockClock starts at zero and moves only when advanced.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/in_memory_block_clock.hpp>

TEST(Small_InMemoryBlockClock, StartsAtZeroAndStaysThereUntilAdvanced)
{
    const SushiDSP::InMemoryBlockClock clock;

    EXPECT_EQ(clock.now_nanoseconds(), 0);
    EXPECT_EQ(clock.now_nanoseconds(), 0);
}

TEST(Small_InMemoryBlockClock, AdvanceMovesTheTimeByExactlyTheAmountGiven)
{
    SushiDSP::InMemoryBlockClock clock;

    clock.advance(1500);
    clock.advance(250);

    EXPECT_EQ(clock.now_nanoseconds(), 1750);
}
