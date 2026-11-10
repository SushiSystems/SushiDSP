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
 * @brief Tests that a delay ring reads a written block back at its delay and holds silence after reset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/delay_ring.hpp>

#include <array>

namespace
{
    /// @brief Counts the frames of every block the tests write and read.
    constexpr std::uint32_t K_FRAMES = 16;

    using Block = std::array<float, K_FRAMES>;

    /// @brief Builds a ring that holds a unit impulse at the first frame of its last block.
    SushiDSP::room::DelayRing ring_with_impulse()
    {
        SushiDSP::room::DelayRing ring;
        ring.prepare(64u, K_FRAMES);
        Block block{};
        block[0] = 1.0f;
        ring.write(block.data(), K_FRAMES);
        return ring;
    }

    /// @brief Reads one block from @p ring at @p delay_frames with unit gain.
    Block read_block(const SushiDSP::room::DelayRing& ring, double delay_frames)
    {
        Block output{};
        ring.read(delay_frames, 1.0f, output.data(), K_FRAMES);
        return output;
    }
} // namespace

TEST(Small_DelayRing, DelayRingReadsAnImpulseBackAtAWholeDelay)
{
    const SushiDSP::room::DelayRing ring = ring_with_impulse();

    // A whole delay of four frames moves the impulse from frame 0 to frame 4 without change.
    const Block expected = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    EXPECT_EQ(read_block(ring, 4.0), expected);
}

TEST(Small_DelayRing, DelayRingInterpolatesAHalfFrameDelayWithTheCubicLagrangeWeights)
{
    const SushiDSP::room::DelayRing ring = ring_with_impulse();

    /* The cubic Lagrange weights at a half-frame fraction are -1/16, 9/16, 9/16 and -1/16, all
       exact in float, centred between frames 4 and 5. */
    const Block expected = {0.0f, 0.0f, 0.0f, -0.0625f, 0.5625f, 0.5625f, -0.0625f};
    EXPECT_EQ(read_block(ring, 4.5), expected);
}

TEST(Small_DelayRing, WriteFollowedByResetLeavesNoEcho)
{
    SushiDSP::room::DelayRing ring = ring_with_impulse();

    ring.reset();

    const Block silence{};
    EXPECT_EQ(read_block(ring, 1.0), silence);
}
