/**************************************************************************/
/* test_in_memory_writer.cpp                                              */
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
 * @file test_in_memory_writer.cpp
 * @brief Tests TrackRecorder's queue and drop decisions against an in-memory take file.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/in_memory_audio_file_writer.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK_FRAMES = 256;
    constexpr std::size_t K_BLOCKS = 32;
    constexpr std::size_t K_UNDERSIZED_RING_FRAMES = 64;
    constexpr std::size_t K_ROOMY_RING_FRAMES = 4096;
    const std::string K_TAKE_PATH = "take.wav";

    /// @brief Writes the same block to a recorder several times and counts the writes it took.
    std::size_t count_accepted_writes(SushiDSP::TrackRecorder& recorder, const float* const* channels,
                                      std::uint32_t frames, std::size_t blocks)
    {
        std::size_t accepted = 0;
        for (std::size_t index = 0; index < blocks; ++index)
        {
            accepted += recorder.write(channels, frames) ? 1u : 0u;
        }
        return accepted;
    }
} // namespace

TEST(Small_TrackRecorder, DropsAndCountsEveryBlockLargerThanTheRing)
{
    SushiDSP::InMemoryAudioFileStore store;
    SushiDSP::TrackRecorder recorder(1u, 48000.0, K_UNDERSIZED_RING_FRAMES, store.opener());
    ASSERT_TRUE(recorder.arm(K_TAKE_PATH));
    std::vector<float> block(K_BLOCK_FRAMES, 0.25f);
    const float* channels[1] = {block.data()};

    const std::size_t accepted = count_accepted_writes(recorder, channels, K_BLOCK_FRAMES, K_BLOCKS);

    EXPECT_EQ(accepted, 0u);
    EXPECT_EQ(recorder.dropped_blocks(), static_cast<std::uint64_t>(K_BLOCKS));
}

TEST(Small_TrackRecorder, WritesNothingOfADroppedBlock)
{
    SushiDSP::InMemoryAudioFileStore store;
    SushiDSP::TrackRecorder recorder(1u, 48000.0, K_UNDERSIZED_RING_FRAMES, store.opener());
    ASSERT_TRUE(recorder.arm(K_TAKE_PATH));
    std::vector<float> block(K_BLOCK_FRAMES, 0.25f);
    const float* channels[1] = {block.data()};
    count_accepted_writes(recorder, channels, K_BLOCK_FRAMES, K_BLOCKS);

    recorder.stop();

    EXPECT_EQ(recorder.frames_written(), 0u);
    ASSERT_NE(store.recording(K_TAKE_PATH), nullptr);
    EXPECT_EQ(store.recording(K_TAKE_PATH)->frames(), 0u);
}

TEST(Small_TrackRecorder, WriteAfterStopIsANoOp)
{
    SushiDSP::InMemoryAudioFileStore store;
    SushiDSP::TrackRecorder recorder(1u, 48000.0, K_ROOMY_RING_FRAMES, store.opener());
    ASSERT_TRUE(recorder.arm(K_TAKE_PATH));
    std::vector<float> block(128, 0.5f);
    const float* channels[1] = {block.data()};
    ASSERT_TRUE(recorder.write(channels, 128u));
    recorder.stop();
    ASSERT_FALSE(recorder.is_recording());
    const std::uint64_t frames_at_stop = recorder.frames_written();

    EXPECT_FALSE(recorder.write(channels, 128u));

    EXPECT_EQ(recorder.frames_written(), frames_at_stop);
    EXPECT_EQ(recorder.dropped_blocks(), 0u);
}

TEST(Small_TrackRecorder, StopDrainsEveryAcceptedFrameIntoTheTakeInOrder)
{
    SushiDSP::InMemoryAudioFileStore store;
    SushiDSP::TrackRecorder recorder(2u, 44100.0, K_ROOMY_RING_FRAMES, store.opener());
    ASSERT_TRUE(recorder.arm(K_TAKE_PATH));
    std::vector<float> left(K_BLOCK_FRAMES);
    std::vector<float> right(K_BLOCK_FRAMES);
    const float* channels[2] = {left.data(), right.data()};
    std::vector<float> expected_left;
    std::vector<float> expected_right;
    for (std::size_t block = 0; block < 4u; ++block)
    {
        for (std::uint32_t frame = 0; frame < K_BLOCK_FRAMES; ++frame)
        {
            left[frame] = static_cast<float>(block * K_BLOCK_FRAMES + frame);
            right[frame] = -left[frame];
        }
        ASSERT_TRUE(recorder.write(channels, K_BLOCK_FRAMES));
        expected_left.insert(expected_left.end(), left.begin(), left.end());
        expected_right.insert(expected_right.end(), right.begin(), right.end());
    }

    recorder.stop();

    const SushiDSP::InMemoryRecording* take = store.recording(K_TAKE_PATH);
    ASSERT_NE(take, nullptr);
    EXPECT_EQ(take->channels, 2u);
    EXPECT_EQ(take->sample_rate, 44100.0);
    EXPECT_TRUE(take->closed);
    EXPECT_EQ(take->samples[0], expected_left);
    EXPECT_EQ(take->samples[1], expected_right);
    EXPECT_EQ(recorder.frames_written(), static_cast<std::uint64_t>(4u * K_BLOCK_FRAMES));
}
