/**************************************************************************/
/* test_block_server.cpp                                                  */
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
 * @file test_block_server.cpp
 * @brief Tests that the block server answers each block with the callback's output, or with silence.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/block_server.hpp>

#include <cstdint>
#include <vector>

namespace
{
    /// @brief Holds a stereo device of 8-frame blocks.
    constexpr SushiDSP::DeviceFormat K_STEREO{48000.0, 8, 0, 2};

    /// @brief Holds a mono device of 8-frame blocks.
    constexpr SushiDSP::DeviceFormat K_MONO{48000.0, 8, 0, 1};

    /// @brief Holds the value the stream holds before a block, which only a served block can clear.
    constexpr float K_STALE = 1.0f;

    /// @brief Holds the left level the stereo callback writes.
    constexpr float K_LEFT = 0.25f;

    /// @brief Holds the right level the stereo callback writes.
    constexpr float K_RIGHT = -0.5f;

    /// @brief Returns a stream of @p frames frames of @p channels channels, all stale.
    std::vector<float> stale_stream(std::uint32_t frames, std::uint32_t channels)
    {
        return std::vector<float>(static_cast<std::size_t>(frames) * channels, K_STALE);
    }

    /// @brief Returns a stream of @p samples samples that are all zero.
    std::vector<float> silent_stream(std::size_t samples)
    {
        return std::vector<float>(samples, 0.0f);
    }

    /// @brief Fills every channel of the block with its own level, as a stereo callback would.
    void write_levels(float* const* outputs, std::uint32_t frames)
    {
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            outputs[0][frame] = K_LEFT;
            outputs[1][frame] = K_RIGHT;
        }
    }

    /// @brief Returns the interleaved stream of @p frames frames of K_LEFT and K_RIGHT.
    std::vector<float> interleaved_levels(std::uint32_t frames)
    {
        std::vector<float> expected;
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            expected.push_back(K_LEFT);
            expected.push_back(K_RIGHT);
        }
        return expected;
    }
} // namespace

TEST(Small_BlockServer, ABlockWithNoCallbackInstalledIsCounted)
{
    SushiDSP::BlockServer server(K_STEREO);
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(server.counters().no_callback.value(), 1U);
}

TEST(Small_BlockServer, ABlockWithNoCallbackInstalledIsAnsweredWithSilence)
{
    SushiDSP::BlockServer server(K_STEREO);
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(stream, silent_stream(16));
}

TEST(Small_BlockServer, ABlockRunsTheCallbackWithItsFrames)
{
    SushiDSP::BlockServer server(K_STEREO);
    std::uint32_t frames_seen = 0;
    server.set_callback([&](const float* const*, float* const*, std::uint32_t frames) { frames_seen = frames; });
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(frames_seen, 8U);
}

TEST(Small_BlockServer, ABlockWithACallbackCountsNoMissingCallbackAndNoOversizeBlock)
{
    SushiDSP::BlockServer server(K_STEREO);
    server.set_callback([](const float* const*, float* const*, std::uint32_t) {});
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(server.counters().no_callback.value(), 0U);
    EXPECT_EQ(server.counters().oversize_block.value(), 0U);
}

TEST(Small_BlockServer, AStereoBlockIsInterleavedFrameByFrame)
{
    SushiDSP::BlockServer server(K_STEREO);
    server.set_callback([](const float* const*, float* const* outputs, std::uint32_t frames)
                        { write_levels(outputs, frames); });
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(stream, interleaved_levels(8));
}

TEST(Small_BlockServer, AMonoBlockIsWrittenStraightIntoTheStream)
{
    SushiDSP::BlockServer server(K_MONO);
    server.set_callback([](const float* const*, float* const* outputs, std::uint32_t frames)
                        { outputs[0][frames - 1] = K_LEFT; });
    std::vector<float> stream = stale_stream(8, 1);

    server.serve(stream.data(), 8);

    EXPECT_EQ(stream.back(), K_LEFT);
}

TEST(Small_BlockServer, ABlockLongerThanTheScratchIsCountedWithItsLength)
{
    SushiDSP::BlockServer server(K_STEREO);
    server.set_callback([](const float* const*, float* const*, std::uint32_t) {});
    std::vector<float> stream = stale_stream(20, 2);

    server.serve(stream.data(), 20);

    EXPECT_EQ(server.counters().oversize_block.value(), 1U);
    EXPECT_EQ(server.counters().oversize_block.peak(), 20U);
}

TEST(Small_BlockServer, ABlockLongerThanTheScratchIsAnsweredWithSilence)
{
    SushiDSP::BlockServer server(K_STEREO);
    server.set_callback([](const float* const*, float* const*, std::uint32_t) {});
    std::vector<float> stream = stale_stream(20, 2);

    server.serve(stream.data(), 20);

    EXPECT_EQ(stream, silent_stream(40));
}

TEST(Small_BlockServer, ClearingTheCallbackSendsTheNextBlockToSilence)
{
    SushiDSP::BlockServer server(K_STEREO);
    server.set_callback([](const float* const*, float* const* outputs, std::uint32_t frames)
                        { write_levels(outputs, frames); });
    server.clear_callback();
    std::vector<float> stream = stale_stream(8, 2);

    server.serve(stream.data(), 8);

    EXPECT_EQ(stream, silent_stream(16));
}
