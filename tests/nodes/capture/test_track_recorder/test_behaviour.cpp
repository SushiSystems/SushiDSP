/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests what TrackRecorder and RecordingBus write to their files.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/recording_bus.hpp>
#include <SushiDSP/nodes/capture/take.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>
#include <SushiDSP/foundation/io/wav_file.hpp>

namespace
{
    constexpr std::uint32_t K_RAMP_BLOCK_FRAMES = 256;
    constexpr std::size_t K_RAMP_BLOCKS = 16;
    constexpr std::uint32_t K_BUS_BLOCK_FRAMES = 128;
    constexpr std::size_t K_BUS_BLOCKS = 20;
    constexpr std::uint32_t K_DROP_BLOCK_FRAMES = 256;
    constexpr std::size_t K_DROP_BLOCKS = 32;

    /// @brief Owns a fresh folder under the temporary directory and removes it on exit, so a
    /// failed assertion leaves nothing behind.
    class TempFolder
    {
        public:
            explicit TempFolder(const std::string& leaf)
                : root_(std::filesystem::temp_directory_path() / ("sushidsp_recorder_" + leaf))
            {
                std::filesystem::remove_all(root_);
                std::filesystem::create_directories(root_);
            }

            ~TempFolder()
            {
                std::filesystem::remove_all(root_);
            }

            TempFolder(const TempFolder&) = delete;
            TempFolder& operator=(const TempFolder&) = delete;

            const std::filesystem::path& path() const
            {
                return root_;
            }

        private:
            std::filesystem::path root_;
    };

    float ramp_sample(std::size_t index)
    {
        return static_cast<float>(index % 1024u) / 1024.0f;
    }

    /// @brief What a recorder reported after a ramp was written to it and it stopped.
    struct RecordedRamp
    {
        bool armed;
        std::uint64_t frames_written;
        std::uint64_t dropped_blocks;
        std::string path;
    };

    /// @brief Records K_RAMP_BLOCKS blocks of a ramp into a mono file and stops the recorder.
    RecordedRamp record_ramp(const TempFolder& folder)
    {
        RecordedRamp result{false, 0u, 0u, (folder.path() / "ramp.wav").string()};
        SushiDSP::TrackRecorder recorder(1u, 48000.0, 8192);
        result.armed = recorder.arm(result.path);

        std::vector<float> block(K_RAMP_BLOCK_FRAMES, 0.0f);
        const float* channels[1] = {block.data()};
        for (std::size_t index = 0; index < K_RAMP_BLOCKS; ++index)
        {
            for (std::uint32_t frame = 0; frame < K_RAMP_BLOCK_FRAMES; ++frame)
            {
                block[frame] = ramp_sample(index * K_RAMP_BLOCK_FRAMES + frame);
            }
            recorder.write(channels, K_RAMP_BLOCK_FRAMES);
        }

        recorder.stop();
        result.frames_written = recorder.frames_written();
        result.dropped_blocks = recorder.dropped_blocks();
        return result;
    }

    /// @brief Writes the same block to a bus several times and counts the writes it took.
    std::size_t count_accepted_bus_writes(SushiDSP::RecordingBus& bus, const float* const* processed,
                                          const float* dry, std::uint32_t frames, std::size_t blocks)
    {
        std::size_t accepted = 0;
        for (std::size_t index = 0; index < blocks; ++index)
        {
            accepted += bus.write(processed, dry, frames) ? 1u : 0u;
        }
        return accepted;
    }

    /// @brief Records K_BUS_BLOCKS blocks into a bus armed in a folder and stops it.
    void record_bus(SushiDSP::RecordingBus& bus, const TempFolder& folder, const char* name)
    {
        std::vector<float> processed(K_BUS_BLOCK_FRAMES, 0.4f);
        std::vector<float> dry(K_BUS_BLOCK_FRAMES, -0.2f);
        const float* processed_channels[1] = {processed.data()};

        bus.arm(folder.path().string(), name);
        count_accepted_bus_writes(bus, processed_channels, dry.data(), K_BUS_BLOCK_FRAMES, K_BUS_BLOCKS);
        bus.stop();
    }
} // namespace

TEST(Medium_TrackRecorder, TrackRecorderRecordsARampSampleForSample)
{
    const TempFolder folder("ramp");
    const RecordedRamp recorded = record_ramp(folder);
    ASSERT_TRUE(recorded.armed);

    const SushiDSP::Take take = SushiDSP::Take::load(recorded.path);

    const std::size_t frames = K_RAMP_BLOCKS * K_RAMP_BLOCK_FRAMES;
    ASSERT_EQ(take.frames(), frames);
    ASSERT_NE(take.channel(0), nullptr);
    for (std::size_t index = 0; index < frames; ++index)
    {
        ASSERT_EQ(take.channel(0)[index], ramp_sample(index)) << "at frame " << index;
    }
}

TEST(Medium_TrackRecorder, TrackRecorderCountsEveryFrameItWrote)
{
    const TempFolder folder("ramp_frames");

    const RecordedRamp recorded = record_ramp(folder);

    EXPECT_EQ(recorded.frames_written, static_cast<std::uint64_t>(K_RAMP_BLOCKS * K_RAMP_BLOCK_FRAMES));
}

TEST(Medium_TrackRecorder, TrackRecorderDropsNoBlockWhenTheRingHoldsIt)
{
    const TempFolder folder("ramp_drops");

    const RecordedRamp recorded = record_ramp(folder);

    EXPECT_EQ(recorded.dropped_blocks, 0u);
}

TEST(Medium_TrackRecorder, TrackRecorderWritesAFileOfItsChannelsAndRate)
{
    const TempFolder folder("ramp_shape");
    const RecordedRamp recorded = record_ramp(folder);

    const SushiDSP::Take take = SushiDSP::Take::load(recorded.path);

    EXPECT_EQ(take.channels(), 1u);
    EXPECT_EQ(take.sample_rate(), 48000.0); // the file stores the rate as the integer 48000
}

TEST(Medium_TrackRecorder, RecordingBusWritesTheSameFrameCountToBothFiles)
{
    const TempFolder folder("bus");
    SushiDSP::RecordingBus bus(1u, 48000.0);

    record_bus(bus, folder, "take01");

    const std::vector<float> processed_file =
        SushiDSP::WavFile::load_mono((folder.path() / "take01-processed.wav").string());
    const std::vector<float> dry_file =
        SushiDSP::WavFile::load_mono((folder.path() / "take01-dry.wav").string());
    EXPECT_EQ(processed_file.size(), dry_file.size());
    EXPECT_EQ(processed_file.size(), K_BUS_BLOCKS * K_BUS_BLOCK_FRAMES);
}

TEST(Medium_TrackRecorder, RecordingBusCountsTheFramesItWroteToEachFile)
{
    const TempFolder folder("bus_counts");
    SushiDSP::RecordingBus bus(1u, 48000.0);

    record_bus(bus, folder, "take01");

    const auto expected = static_cast<std::uint64_t>(K_BUS_BLOCKS * K_BUS_BLOCK_FRAMES);
    EXPECT_EQ(bus.processed_frames_written(), expected);
    EXPECT_EQ(bus.dry_frames_written(), expected);
}

TEST(Medium_TrackRecorder, RecordingBusCountsEveryBlockItTakes)
{
    const TempFolder folder("bus_blocks");
    SushiDSP::RecordingBus bus(1u, 48000.0);

    record_bus(bus, folder, "take01");

    EXPECT_EQ(bus.block_index(), static_cast<std::uint64_t>(K_BUS_BLOCKS));
}

TEST(Medium_TrackRecorder, RecordingBusDropsNoBlockWhenBothRingsHaveRoom)
{
    const TempFolder folder("bus_room");
    SushiDSP::RecordingBus bus(1u, 48000.0);

    record_bus(bus, folder, "take01");

    EXPECT_EQ(bus.dropped_blocks(), 0u);
}

TEST(Medium_TrackRecorder, RecordingBusKeepsBothFilesEqualWhenABlockIsLargerThanOneRing)
{
    const TempFolder folder("bus_drop");
    SushiDSP::RecordingBus bus(1u, 48000.0, 64, SushiDSP::RecordingBus::K_RING_CAPACITY_FRAMES);
    ASSERT_TRUE(bus.arm(folder.path().string(), "take02"));
    std::vector<float> processed(K_DROP_BLOCK_FRAMES, 0.4f);
    std::vector<float> dry(K_DROP_BLOCK_FRAMES, -0.2f);
    const float* processed_channels[1] = {processed.data()};

    const std::size_t accepted =
        count_accepted_bus_writes(bus, processed_channels, dry.data(), K_DROP_BLOCK_FRAMES, K_DROP_BLOCKS);
    bus.stop();

    EXPECT_EQ(accepted, 0u);
    EXPECT_EQ(bus.dropped_blocks(), static_cast<std::uint64_t>(K_DROP_BLOCKS));
    EXPECT_EQ(bus.processed_frames_written(), bus.dry_frames_written());
    EXPECT_EQ(bus.dry_frames_written(), 0u);
}

TEST(Medium_TrackRecorder, RecordingBusArmOnAnUnwritableFolderReturnsFalse)
{
    const TempFolder folder("unwritable");
    const std::filesystem::path missing = folder.path() / "no_such_directory";
    SushiDSP::RecordingBus bus(1u, 48000.0);

    EXPECT_FALSE(bus.arm(missing.string(), "take01"));

    EXPECT_FALSE(bus.is_recording());
}
