/**************************************************************************/
/* test_delay_ring.cpp                                                    */
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
 * @file test_delay_ring.cpp
 * @brief Tests that a delay ring holds silence after reset and reads back zero from it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/delay_ring.hpp>

#include <array>

TEST(Small_DelayRing, ReadAfterResetGivesSilence)
{
    SushiDSP::room::DelayRing ring;
    ring.prepare(64u, 16u);
    ring.reset();
    std::array<float, 8> output{};
    ring.read(4.0, 1.0f, output.data(), 8u);
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(Small_DelayRing, WriteFollowedByResetLeavesNoEcho)
{
    SushiDSP::room::DelayRing ring;
    ring.prepare(64u, 16u);
    ring.reset();
    std::array<float, 16> block{};
    block.fill(0.75f);
    ring.write(block.data(), 16u);
    ring.reset();
    std::array<float, 8> output{};
    ring.read(0.0, 1.0f, output.data(), 8u);
    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}
