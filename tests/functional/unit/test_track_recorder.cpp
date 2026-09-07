/**************************************************************************/
/* test_track_recorder.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/RecordingBus.hpp"
#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TrackRecorder.hpp"
#include "SushiDSP/io/WavFile.hpp"

namespace
{
    std::filesystem::path make_fixture(const std::string& leaf)
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("sushidsp_recorder_" + leaf);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
    }

    float ramp_sample(std::size_t index)
    {
        return static_cast<float>(index % 1024u) / 1024.0f;
    }
}

TEST(Unit_Dsp, TrackRecorderRecordsARampSampleForSample)
{
    const std::filesystem::path root = make_fixture("ramp");
    const std::string           path = (root / "ramp.wav").string();

    constexpr std::uint32_t kBlockFrames = 256;
    constexpr std::size_t   kBlocks = 16;

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(kBlockFrames, 0.0f);
    const float*       channels[1] = { block.data() };

    for (std::size_t index = 0; index < kBlocks; ++index)
    {
        for (std::uint32_t frame = 0; frame < kBlockFrames; ++frame)
        {
            block[frame] = ramp_sample(index * kBlockFrames + frame);
        }
        ASSERT_TRUE(recorder.write(channels, kBlockFrames));
    }

    recorder.stop();

    const std::size_t expected_frames = kBlocks * kBlockFrames;
    EXPECT_EQ(recorder.frames_written(), static_cast<std::uint64_t>(expected_frames));
    EXPECT_EQ(recorder.dropped_blocks(), 0u);

    const SushiDSP::Take take = SushiDSP::Take::load(path);
    ASSERT_EQ(take.frames(), expected_frames);
    EXPECT_EQ(take.channels(), 1u);
    EXPECT_DOUBLE_EQ(take.sample_rate(), 48000.0);

    const float* samples = take.channel(0);
    ASSERT_NE(samples, nullptr);
    for (std::size_t index = 0; index < expected_frames; ++index)
    {
        ASSERT_EQ(samples[index], ramp_sample(index)) << "at frame " << index;
    }

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, TrackRecorderDropsBlocksLargerThanTheRing)
{
    const std::filesystem::path root = make_fixture("undersized");
    const std::string           path = (root / "undersized.wav").string();

    constexpr std::uint32_t kBlockFrames = 256;
    constexpr std::size_t   kBlocks = 32;

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 64);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(kBlockFrames, 0.25f);
    const float*       channels[1] = { block.data() };

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < kBlocks; ++index)
    {
        EXPECT_FALSE(recorder.write(channels, kBlockFrames));
    }
    const std::chrono::milliseconds elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

    EXPECT_EQ(recorder.dropped_blocks(), static_cast<std::uint64_t>(kBlocks));
    EXPECT_LT(elapsed.count(), 100);

    recorder.stop();
    EXPECT_EQ(recorder.frames_written(), 0u);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, TrackRecorderWriteAfterStopIsANoOp)
{
    const std::filesystem::path root = make_fixture("after_stop");
    const std::string           path = (root / "after_stop.wav").string();

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 4096);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(128, 0.5f);
    const float*       channels[1] = { block.data() };
    ASSERT_TRUE(recorder.write(channels, 128u));

    recorder.stop();
    EXPECT_FALSE(recorder.is_recording());

    const std::uint64_t frames_at_stop = recorder.frames_written();
    EXPECT_FALSE(recorder.write(channels, 128u));
    EXPECT_EQ(recorder.frames_written(), frames_at_stop);
    EXPECT_EQ(recorder.dropped_blocks(), 0u);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, RecordingBusWritesTwoFilesOfEqualLength)
{
    const std::filesystem::path root = make_fixture("bus");

    constexpr std::uint32_t kBlockFrames = 128;
    constexpr std::size_t   kBlocks = 20;

    SushiDSP::RecordingBus bus(1u, 48000.0);
    ASSERT_TRUE(bus.arm(root.string(), "take01"));
    EXPECT_EQ(bus.armed_block_index(), 0u);

    std::vector<float> processed(kBlockFrames, 0.4f);
    std::vector<float> dry(kBlockFrames, -0.2f);
    const float*       processed_channels[1] = { processed.data() };
    const float*       dry_channels[1] = { dry.data() };

    for (std::size_t index = 0; index < kBlocks; ++index)
    {
        ASSERT_TRUE(bus.write_processed(processed_channels, kBlockFrames));
        ASSERT_TRUE(bus.write_dry(dry_channels, kBlockFrames));
    }

    bus.stop();

    EXPECT_EQ(bus.dropped_blocks(), 0u);
    EXPECT_EQ(bus.processed_frames_written(), bus.dry_frames_written());
    EXPECT_EQ(bus.processed_frames_written(),
              static_cast<std::uint64_t>(kBlocks * kBlockFrames));
    EXPECT_EQ(bus.block_index(), static_cast<std::uint64_t>(kBlocks));

    const std::vector<float> processed_file =
        SushiDSP::WavFile::load_mono((root / "take01-processed.wav").string());
    const std::vector<float> dry_file =
        SushiDSP::WavFile::load_mono((root / "take01-dry.wav").string());
    EXPECT_EQ(processed_file.size(), dry_file.size());
    EXPECT_EQ(processed_file.size(), kBlocks * kBlockFrames);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, RecordingBusArmOnAnUnwritableFolderReturnsFalse)
{
    const std::filesystem::path root = make_fixture("unwritable");
    const std::filesystem::path missing = root / "no_such_directory";

    SushiDSP::RecordingBus bus(1u, 48000.0);
    EXPECT_FALSE(bus.arm(missing.string(), "take01"));
    EXPECT_FALSE(bus.is_recording());

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 1024);
    EXPECT_FALSE(recorder.arm((missing / "take01.wav").string()));
    EXPECT_FALSE(recorder.is_recording());

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, TakeAllocatesItsChannelsAndReportsItsShape)
{
    SushiDSP::Take take(2u, 96000.0, 512);

    EXPECT_EQ(take.channels(), 2u);
    EXPECT_EQ(take.frames(), 512u);
    EXPECT_DOUBLE_EQ(take.sample_rate(), 96000.0);
    ASSERT_NE(take.channel(0), nullptr);
    ASSERT_NE(take.channel(1), nullptr);
    EXPECT_EQ(take.channel(2), nullptr);
    EXPECT_EQ(take.mutable_channel(2), nullptr);

    take.mutable_channel(1)[7] = 0.75f;
    EXPECT_EQ(take.channel(1)[7], 0.75f);
    EXPECT_EQ(take.channel(0)[7], 0.0f);
}
