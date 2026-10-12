/**************************************************************************/
/* test_track_recorder.cpp                                                */
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
 * @file test_track_recorder.cpp
 * @brief Tests the recording paths of TrackRecorder and RecordingBus.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/recording_bus.hpp"
#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/capture/track_recorder.hpp"
#include "SushiDSP/io/wav_file.hpp"

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
} // namespace

TEST(Unit_Dsp, TrackRecorderRecordsARampSampleForSample)
{
    const std::filesystem::path root = make_fixture("ramp");
    const std::string path = (root / "ramp.wav").string();

    constexpr std::uint32_t K_BLOCK_FRAMES = 256;
    constexpr std::size_t K_BLOCKS = 16;

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(K_BLOCK_FRAMES, 0.0f);
    const float* channels[1] = {block.data()};

    for (std::size_t index = 0; index < K_BLOCKS; ++index)
    {
        for (std::uint32_t frame = 0; frame < K_BLOCK_FRAMES; ++frame)
        {
            block[frame] = ramp_sample(index * K_BLOCK_FRAMES + frame);
        }
        ASSERT_TRUE(recorder.write(channels, K_BLOCK_FRAMES));
    }

    recorder.stop();

    const std::size_t expected_frames = K_BLOCKS * K_BLOCK_FRAMES;
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
    const std::string path = (root / "undersized.wav").string();

    constexpr std::uint32_t K_BLOCK_FRAMES = 256;
    constexpr std::size_t K_BLOCKS = 32;

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 64);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(K_BLOCK_FRAMES, 0.25f);
    const float* channels[1] = {block.data()};

    for (std::size_t index = 0; index < K_BLOCKS; ++index)
    {
        EXPECT_FALSE(recorder.write(channels, K_BLOCK_FRAMES));
    }

    EXPECT_EQ(recorder.dropped_blocks(), static_cast<std::uint64_t>(K_BLOCKS));

    recorder.stop();
    EXPECT_EQ(recorder.frames_written(), 0u);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, TrackRecorderWriteAfterStopIsANoOp)
{
    const std::filesystem::path root = make_fixture("after_stop");
    const std::string path = (root / "after_stop.wav").string();

    SushiDSP::TrackRecorder recorder(1u, 48000.0, 4096);
    ASSERT_TRUE(recorder.arm(path));

    std::vector<float> block(128, 0.5f);
    const float* channels[1] = {block.data()};
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

    constexpr std::uint32_t K_BLOCK_FRAMES = 128;
    constexpr std::size_t K_BLOCKS = 20;

    SushiDSP::RecordingBus bus(1u, 48000.0);
    ASSERT_TRUE(bus.arm(root.string(), "take01"));

    std::vector<float> processed(K_BLOCK_FRAMES, 0.4f);
    std::vector<float> dry(K_BLOCK_FRAMES, -0.2f);
    const float* processed_channels[1] = {processed.data()};

    for (std::size_t index = 0; index < K_BLOCKS; ++index)
    {
        ASSERT_TRUE(bus.write(processed_channels, dry.data(), K_BLOCK_FRAMES));
    }

    bus.stop();

    EXPECT_EQ(bus.dropped_blocks(), 0u);
    EXPECT_EQ(bus.processed_frames_written(), bus.dry_frames_written());
    EXPECT_EQ(bus.processed_frames_written(),
              static_cast<std::uint64_t>(K_BLOCKS * K_BLOCK_FRAMES));
    EXPECT_EQ(bus.block_index(), static_cast<std::uint64_t>(K_BLOCKS));

    const std::vector<float> processed_file =
        SushiDSP::WavFile::load_mono((root / "take01-processed.wav").string());
    const std::vector<float> dry_file =
        SushiDSP::WavFile::load_mono((root / "take01-dry.wav").string());
    EXPECT_EQ(processed_file.size(), dry_file.size());
    EXPECT_EQ(processed_file.size(), K_BLOCKS * K_BLOCK_FRAMES);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, RecordingBusKeepsBothFilesEqualWhenOneRingFills)
{
    const std::filesystem::path root = make_fixture("bus_drop");

    constexpr std::uint32_t K_BLOCK_FRAMES = 256;
    constexpr std::size_t K_BLOCKS = 32;

    SushiDSP::RecordingBus bus(1u, 48000.0, 64, SushiDSP::RecordingBus::K_RING_CAPACITY_FRAMES);
    ASSERT_TRUE(bus.arm(root.string(), "take02"));

    std::vector<float> processed(K_BLOCK_FRAMES, 0.4f);
    std::vector<float> dry(K_BLOCK_FRAMES, -0.2f);
    const float* processed_channels[1] = {processed.data()};

    for (std::size_t index = 0; index < K_BLOCKS; ++index)
    {
        EXPECT_FALSE(bus.write(processed_channels, dry.data(), K_BLOCK_FRAMES));
    }

    bus.stop();

    EXPECT_EQ(bus.dropped_blocks(), static_cast<std::uint64_t>(K_BLOCKS));
    EXPECT_EQ(bus.processed_frames_written(), bus.dry_frames_written());
    EXPECT_EQ(bus.dry_frames_written(), 0u);
    EXPECT_EQ(bus.block_index(), static_cast<std::uint64_t>(K_BLOCKS));

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
