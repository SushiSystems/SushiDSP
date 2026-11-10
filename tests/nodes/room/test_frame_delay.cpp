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
 * @file test_frame_delay.cpp
 * @brief Tests that a frame delay holds its input back by the number of frames it was prepared with.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/frame_delay.hpp>

#include <array>
#include <cstdint>

namespace
{
    constexpr std::uint32_t K_DELAY = 3u;
    constexpr std::uint32_t K_BLOCK = 4u;

    using Block = std::array<float, K_BLOCK>;

    /// @brief Returns a delay prepared for K_DELAY frames and blocks of K_BLOCK frames, line cleared.
    SushiDSP::room::FrameDelay make_delay()
    {
        SushiDSP::room::FrameDelay delay;
        delay.prepare(K_DELAY, K_BLOCK);
        delay.reset();
        return delay;
    }
} // namespace

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
    SushiDSP::room::FrameDelay delay = make_delay();
    const Block loud{1.0f, 1.0f, 1.0f, 1.0f};
    const Block silence{};
    Block output{};
    delay.process(loud.data(), output.data(), K_BLOCK);
    delay.reset();

    delay.process(silence.data(), output.data(), K_BLOCK);

    EXPECT_EQ(output, silence);
}

TEST(Small_FrameDelay, TheDelayCarriesAcrossBlockBoundaries)
{
    SushiDSP::room::FrameDelay delay = make_delay();
    const Block first{1.0f, 2.0f, 3.0f, 4.0f};
    const Block silence{};
    Block output{};

    delay.process(first.data(), output.data(), K_BLOCK);
    EXPECT_EQ(output, (Block{0.0f, 0.0f, 0.0f, 1.0f}));

    delay.process(silence.data(), output.data(), K_BLOCK);
    EXPECT_EQ(output, (Block{2.0f, 3.0f, 4.0f, 0.0f}));
}

TEST(Small_FrameDelay, InputAndOutputMayAlias)
{
    SushiDSP::room::FrameDelay delay = make_delay();
    Block block{1.0f, 2.0f, 3.0f, 4.0f};

    delay.process(block.data(), block.data(), K_BLOCK);

    EXPECT_EQ(block, (Block{0.0f, 0.0f, 0.0f, 1.0f}));
}
