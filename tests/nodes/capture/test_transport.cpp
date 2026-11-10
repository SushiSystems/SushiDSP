/**************************************************************************/
/* test_transport.cpp                                                     */
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
 * @file test_transport.cpp
 * @brief Tests how Transport keeps its position, speed and loop.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/transport.hpp>

namespace
{
    /// @brief Names a speed, the blocks advanced at it and the position they must reach.
    struct SpeedRow
    {
        const char* name;
        double speed;
        std::uint32_t blocks;
        std::uint32_t block_frames;
        std::uint64_t position;
    };

    /// @brief Names a loop range handed to set_loop() and what the transport then reports.
    struct LoopRow
    {
        const char* name;
        std::uint64_t begin;
        std::uint64_t end;
        bool has_loop;
        std::uint64_t reported_begin;
        std::uint64_t reported_end;
    };

    /// @brief Advances a transport by the same block several times.
    void advance_blocks(SushiDSP::Transport& transport, std::uint32_t blocks, std::uint32_t frames)
    {
        for (std::uint32_t block = 0; block < blocks; ++block)
        {
            transport.advance(frames);
        }
    }
} // namespace

class Small_TransportSpeed : public ::testing::TestWithParam<SpeedRow>
{
};

TEST_P(Small_TransportSpeed, TransportHoldsTheSpeedExactlyOverManyBlocks)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(GetParam().speed);

    advance_blocks(transport, GetParam().blocks, GetParam().block_frames);

    EXPECT_EQ(transport.position_frames(), GetParam().position);
}

// 0.5 * 25600 frames, 0.6 * 64000 frames and 1.3 * 25600000 frames, each to the frame with no drift.
INSTANTIATE_TEST_SUITE_P(
    Rows, Small_TransportSpeed,
    ::testing::Values(SpeedRow{"HalfSpeed", 0.5, 100u, 256u, 12800u},
                      SpeedRow{"SixTenthsSpeed", 0.6, 1000u, 64u, 38400u},
                      SpeedRow{"OnePointThreeSpeed", 1.3, 100000u, 256u, 33280000u}),
    [](const ::testing::TestParamInfo<SpeedRow>& info) { return std::string(info.param.name); });

TEST(Small_Transport, TransportWrapsTheLoopOnTheMusicalPositionAtASpeed)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_loop(1000u, 5000u);
    transport.seek(1000u);
    transport.set_speed(1.5);

    advance_blocks(transport, 11u, 256u);

    EXPECT_EQ(transport.position_frames(), 1224u); // 1000 + (11 * 384) % 4000
}

TEST(Small_Transport, TransportClampsASpeedOutsideItsRange)
{
    SushiDSP::Transport transport;
    transport.set_speed(10.0);
    EXPECT_DOUBLE_EQ(transport.speed(), SushiDSP::Transport::K_MAX_SPEED);
    transport.set_speed(0.1);
    EXPECT_DOUBLE_EQ(transport.speed(), SushiDSP::Transport::K_MIN_SPEED);
}

TEST(Small_Transport, TransportSeekClearsTheSpeedFraction)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.3);
    transport.advance(64u); // 19.2 frames: 19 move and 0.2 stays as the remainder
    transport.seek(1000u);

    // 3 frames at 0.3 are 0.9; a remainder of 0.2 left from before the seek would make it 1.1.
    transport.advance(3u);

    EXPECT_EQ(transport.position_frames(), 1000u);
}

TEST(Small_Transport, TransportAtUnitySpeedAdvancesExactlyAsBefore)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.advance(20u);
    transport.advance(45u);
    transport.advance(100u);
    transport.advance(128u);
    EXPECT_EQ(transport.position_frames(), 293u);
}

TEST(Small_Transport, TransportClearLoopRemovesTheWrap)
{
    SushiDSP::Transport transport;
    transport.set_loop(10, 50);
    transport.seek(30);

    transport.clear_loop();
    transport.advance(100);

    EXPECT_FALSE(transport.has_loop());
    EXPECT_EQ(transport.position_frames(), 130u);
}

TEST(Small_Transport, TransportMovesThroughItsThreeStates)
{
    SushiDSP::Transport transport;
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Stopped);

    transport.play();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Playing);
    transport.record();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Recording);
    transport.stop();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Stopped);
}

TEST(Small_Transport, TransportSeekAndAdvanceMoveThePosition)
{
    SushiDSP::Transport transport;

    transport.seek(4096);
    transport.advance(128);

    EXPECT_EQ(transport.position_frames(), 4224u);
}

class Small_TransportLoopRange : public ::testing::TestWithParam<LoopRow>
{
};

TEST_P(Small_TransportLoopRange, TransportSetLoopTakesOnlyANonEmptyRangeThatFitsTheWord)
{
    SushiDSP::Transport transport;

    transport.set_loop(GetParam().begin, GetParam().end);

    EXPECT_EQ(transport.has_loop(), GetParam().has_loop);
    EXPECT_EQ(transport.loop_begin_frames(), GetParam().reported_begin);
    EXPECT_EQ(transport.loop_end_frames(), GetParam().reported_end);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_TransportLoopRange,
    ::testing::Values(LoopRow{"AValidRange", 100u, 200u, true, 100u, 200u},
                      LoopRow{"AnEmptyRange", 200u, 200u, false, 0u, 0u},
                      LoopRow{"ARangeLongerThanTheWord", 0u, 0x1'0000'0000ull, false, 0u, 0u},
                      LoopRow{"ARangeJustInsideTheWord", 1u, 0x1'0000'0000ull, true, 1u,
                              0x1'0000'0000ull}),
    [](const ::testing::TestParamInfo<LoopRow>& info) { return std::string(info.param.name); });
