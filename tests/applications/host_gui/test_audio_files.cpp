/**************************************************************************/
/* test_audio_files.cpp                                                   */
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
 * @file test_audio_files.cpp
 * @brief Tests the contract of the AudioFiles port against its in-memory and its disk adapter.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/wav_writer.hpp>
#include <SushiDSP/nodes/capture/take.hpp>

#include "audio_files.hpp"
#include "support/remove_at_exit.hpp"
#include "support/wav_fixture.hpp"

namespace
{
    using SushiDSP::host::AudioFiles;
    using Format = SushiDSP::WavWriter::Format;

    /// @brief Writes one block of a mono or stereo take through the opener of a format and closes
    /// the file.
    /// @param channels One vector per channel, one or two of them, all of the same length.
    void write_channels(AudioFiles& files, const std::string& path, Format format, double rate,
                        const std::vector<std::vector<float>>& channels)
    {
        std::unique_ptr<SushiDSP::AudioFileWriter> writer =
            files.writer_opener(format)(path, rate, static_cast<std::uint32_t>(channels.size()));
        const float* pointers[2] = {channels[0].data(), channels.back().data()};
        writer->append(pointers, static_cast<std::uint32_t>(channels[0].size()));
        writer->close();
    }

    /// @brief Reads one channel of a take.
    std::vector<float> channel_of(const SushiDSP::Take& take, std::uint32_t channel)
    {
        return std::vector<float>(take.channel(channel), take.channel(channel) + take.frames());
    }

    void expect_a_float32_file_reads_back_as_written(AudioFiles& files, const std::string& path)
    {
        const std::vector<std::vector<float>> written = {{0.5f, -0.25f, 0.125f},
                                                         {1.0f, 0.0f, -0.75f}};
        write_channels(files, path, Format::Float32, 44100.0, written);

        const SushiDSP::Take take = files.read(path);

        EXPECT_EQ(take.channels(), 2u);
        EXPECT_EQ(take.sample_rate(), 44100.0);
        EXPECT_EQ(take.frames(), 3u);
        EXPECT_EQ(channel_of(take, 0), written[0]);
        EXPECT_EQ(channel_of(take, 1), written[1]);
    }

    // 0.5, -0.5 and 0.25 are 16384, -16384 and 8192 over 32768; 2.0 clamps to 32767 over 32768.
    void expect_a_pcm16_file_holds_the_sixteen_bit_value_of_each_sample(AudioFiles& files,
                                                                         const std::string& path)
    {
        write_channels(files, path, Format::Pcm16, 48000.0, {{0.5f, -0.5f, 0.25f, 2.0f}});

        const SushiDSP::Take take = files.read(path);

        EXPECT_EQ(channel_of(take, 0),
                  (std::vector<float>{0.5f, -0.5f, 0.25f, 32767.0f / 32768.0f}));
    }

    void expect_opening_a_path_again_replaces_the_file(AudioFiles& files, const std::string& path)
    {
        write_channels(files, path, Format::Float32, 48000.0, {{0.5f, 0.5f, 0.5f, 0.5f}});
        write_channels(files, path, Format::Float32, 48000.0, {{0.25f, 0.125f}});

        const SushiDSP::Take take = files.read(path);

        EXPECT_EQ(channel_of(take, 0), (std::vector<float>{0.25f, 0.125f}));
    }

    void expect_reading_a_missing_file_throws(AudioFiles& files, const std::string& path)
    {
        EXPECT_THROW(files.read(path), std::runtime_error);
    }

    void expect_a_removed_file_can_no_longer_be_read(AudioFiles& files, const std::string& path)
    {
        write_channels(files, path, Format::Float32, 48000.0, {{0.5f}});

        const std::string refusal = files.remove(path);

        EXPECT_EQ(refusal, "");
        EXPECT_THROW(files.read(path), std::runtime_error);
    }

    void expect_removing_a_missing_file_reports_nothing(AudioFiles& files, const std::string& path)
    {
        EXPECT_EQ(files.remove(path), "");
    }
} // namespace

TEST(Small_AudioFilesContract, InMemoryFloat32FileReadsBackAsWritten)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_a_float32_file_reads_back_as_written(files, "memory/float32.wav");
}

TEST(Medium_AudioFilesContract, DiskFloat32FileReadsBackAsWritten)
{
    SushiDSP::host::DiskAudioFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_float32.wav");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_a_float32_file_reads_back_as_written(files, path);
}

TEST(Small_AudioFilesContract, InMemoryPcm16FileHoldsTheSixteenBitValueOfEachSample)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_a_pcm16_file_holds_the_sixteen_bit_value_of_each_sample(files, "memory/pcm16.wav");
}

TEST(Medium_AudioFilesContract, DiskPcm16FileHoldsTheSixteenBitValueOfEachSample)
{
    SushiDSP::host::DiskAudioFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_pcm16.wav");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_a_pcm16_file_holds_the_sixteen_bit_value_of_each_sample(files, path);
}

TEST(Small_AudioFilesContract, InMemoryOpeningAPathAgainReplacesTheFile)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_opening_a_path_again_replaces_the_file(files, "memory/replaced.wav");
}

TEST(Medium_AudioFilesContract, DiskOpeningAPathAgainReplacesTheFile)
{
    SushiDSP::host::DiskAudioFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_replaced.wav");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_opening_a_path_again_replaces_the_file(files, path);
}

TEST(Small_AudioFilesContract, InMemoryReadingAMissingFileThrows)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_reading_a_missing_file_throws(files, "memory/missing.wav");
}

TEST(Medium_AudioFilesContract, DiskReadingAMissingFileThrows)
{
    SushiDSP::host::DiskAudioFiles files;
    expect_reading_a_missing_file_throws(files,
                                         TestSupport::temp_fixture_path("contract_missing.wav"));
}

TEST(Small_AudioFilesContract, InMemoryARemovedFileCanNoLongerBeRead)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_a_removed_file_can_no_longer_be_read(files, "memory/removed.wav");
}

TEST(Medium_AudioFilesContract, DiskARemovedFileCanNoLongerBeRead)
{
    SushiDSP::host::DiskAudioFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_removed.wav");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_a_removed_file_can_no_longer_be_read(files, path);
}

TEST(Small_AudioFilesContract, InMemoryRemovingAMissingFileReportsNothing)
{
    SushiDSP::host::InMemoryAudioFiles files;
    expect_removing_a_missing_file_reports_nothing(files, "memory/never_there.wav");
}

TEST(Medium_AudioFilesContract, DiskRemovingAMissingFileReportsNothing)
{
    SushiDSP::host::DiskAudioFiles files;
    expect_removing_a_missing_file_reports_nothing(
        files, TestSupport::temp_fixture_path("contract_never_there.wav"));
}

TEST(Small_InMemoryAudioFiles, AnAddedFileReadsBackWithItsRateAndChannels)
{
    SushiDSP::host::InMemoryAudioFiles files;
    files.add("memory/added.wav", 32000.0, {{0.5f, 0.25f}, {-0.5f, -0.25f}});

    const SushiDSP::Take take = files.read("memory/added.wav");

    EXPECT_EQ(take.sample_rate(), 32000.0);
    EXPECT_EQ(channel_of(take, 1), (std::vector<float>{-0.5f, -0.25f}));
}

TEST(Small_InMemoryAudioFiles, AnUnreadableFileExistsAndDoesNotParse)
{
    SushiDSP::host::InMemoryAudioFiles files;
    files.add_unreadable("memory/garbage.wav");

    EXPECT_TRUE(files.exists("memory/garbage.wav"));
    EXPECT_THROW(files.read("memory/garbage.wav"), std::runtime_error);
}

TEST(Small_InMemoryAudioFiles, AWriteToARefusedPathThrows)
{
    SushiDSP::host::InMemoryAudioFiles files;
    files.refuse_writing("memory/locked.wav");

    EXPECT_THROW(files.writer_opener(Format::Float32)("memory/locked.wav", 48000.0, 1u),
                 std::runtime_error);
    EXPECT_FALSE(files.exists("memory/locked.wav"));
}
