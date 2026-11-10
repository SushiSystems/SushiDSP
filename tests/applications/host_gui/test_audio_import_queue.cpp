/**************************************************************************/
/* test_audio_import_queue.cpp                                            */
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
 * @file test_audio_import_queue.cpp
 * @brief Covers the worker that decodes a dropped song while the GUI keeps drawing.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "audio_import_queue.hpp"

namespace
{
    using SushiDSP::host::AudioImport;
    using SushiDSP::host::AudioImportQueue;
    using SushiDSP::host::InMemoryAudioImporter;

    constexpr double K_RATE = 48000.0;

    /// @brief Waits for the worker to finish what was requested, then takes every result.
    /// @param queue The queue to drain.
    /// @return The finished imports, in the order they finished.
    std::vector<AudioImport> drain_when_idle(AudioImportQueue& queue)
    {
        queue.wait_until_idle();
        std::vector<AudioImport> drained;
        queue.drain(drained);
        return drained;
    }
} // namespace

TEST(Medium_AudioImportQueue, AudioImportQueueReportsAFailureAndKeepsThePlacementItWasGiven)
{
    InMemoryAudioImporter importer;
    importer.fail("songs/missing.mp3", "no such file");
    AudioImportQueue queue(importer);
    queue.request("songs/missing.mp3", 5, 987654, K_RATE);

    const std::vector<AudioImport> drained = drain_when_idle(queue);

    ASSERT_EQ(drained.size(), 1u);
    EXPECT_EQ(drained[0].source_path, "songs/missing.mp3");
    EXPECT_EQ(drained[0].track, 5u);
    EXPECT_EQ(drained[0].start_frame, 987654u);
    EXPECT_TRUE(drained[0].wav_path.empty());
    EXPECT_EQ(drained[0].error, "no such file");
}

TEST(Medium_AudioImportQueue, AudioImportQueueHandsBackOneResultPerRequest)
{
    InMemoryAudioImporter importer;
    AudioImportQueue queue(importer);
    queue.request("songs/one.mp3", 0, 0, K_RATE);
    queue.request("songs/two.mp3", 1, 64, K_RATE);
    queue.request("songs/three.mp3", 2, 128, K_RATE);

    const std::vector<AudioImport> drained = drain_when_idle(queue);

    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(drained[0].wav_path, "songs/one.mp3.wav");
    EXPECT_EQ(drained[0].track, 0u);
    EXPECT_EQ(drained[1].wav_path, "songs/two.mp3.wav");
    EXPECT_EQ(drained[1].track, 1u);
    EXPECT_EQ(drained[2].wav_path, "songs/three.mp3.wav");
    EXPECT_EQ(drained[2].track, 2u);
    EXPECT_EQ(queue.pending(), 0u);
}
