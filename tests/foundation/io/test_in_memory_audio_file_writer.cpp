/**************************************************************************/
/* test_in_memory_audio_file_writer.cpp                                   */
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
 * @file test_in_memory_audio_file_writer.cpp
 * @brief Tests that the in-memory store keeps what a writer appends, per path and per channel.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/in_memory_audio_file_writer.hpp>

TEST(Small_InMemoryAudioFileStore, KeepsWhatTwoAppendsWroteToEachChannelInOrder)
{
    SushiDSP::InMemoryAudioFileStore store;
    const SushiDSP::AudioFileWriterOpener open = store.opener();
    std::unique_ptr<SushiDSP::AudioFileWriter> writer = open("a.wav", 44100.0, 2u);
    const std::vector<float> left = {1.0f, 2.0f, 3.0f};
    const std::vector<float> right = {-1.0f, -2.0f, -3.0f};
    const float* first[2] = {left.data(), right.data()};
    const float* second[2] = {left.data() + 2, right.data() + 2};

    writer->append(first, 2u);
    writer->append(second, 1u);

    const SushiDSP::InMemoryRecording* recording = store.recording("a.wav");
    ASSERT_NE(recording, nullptr);
    EXPECT_EQ(recording->samples[0], left);
    EXPECT_EQ(recording->samples[1], right);
    EXPECT_EQ(recording->sample_rate, 44100.0);
    EXPECT_EQ(recording->channels, 2u);
    EXPECT_EQ(writer->frames_written(), 3u);
}

TEST(Small_InMemoryAudioFileStore, AppendAfterCloseChangesNothing)
{
    SushiDSP::InMemoryAudioFileStore store;
    std::unique_ptr<SushiDSP::AudioFileWriter> writer = store.opener()("a.wav", 48000.0, 1u);
    const std::vector<float> block = {0.5f, 0.5f};
    const float* channels[1] = {block.data()};
    writer->append(channels, 2u);
    writer->close();

    writer->append(channels, 2u);

    EXPECT_EQ(writer->frames_written(), 2u);
    EXPECT_TRUE(store.recording("a.wav")->closed);
}

TEST(Small_InMemoryAudioFileStore, ARefusedPathThrowsAndLeavesNoRecording)
{
    SushiDSP::InMemoryAudioFileStore store;
    store.refuse("no/such/dir/a.wav");

    EXPECT_THROW(store.opener()("no/such/dir/a.wav", 48000.0, 1u), std::runtime_error);

    EXPECT_EQ(store.recording("no/such/dir/a.wav"), nullptr);
}

TEST(Small_InMemoryAudioFileStore, AnUnopenedPathHasNoRecording)
{
    const SushiDSP::InMemoryAudioFileStore store;

    EXPECT_EQ(store.recording("never.wav"), nullptr);
}
