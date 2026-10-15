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
 * @file test_audio_import_queue.cpp
 * @brief Covers the worker that decodes a dropped song while the GUI keeps drawing.
 * @author Mustafa Garip & Sushi Systems
 */

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "audio_import_queue.hpp"

namespace
{
    /// @brief How long a test waits for the worker before giving up.
    constexpr std::chrono::seconds K_PATIENCE{10};

    /// @brief Waits for the queue to report one finished import.
    /// @param queue The queue to drain.
    /// @param out Receives the finished imports.
    /// @return True when an import arrived before K_PATIENCE ran out.
    bool wait_for_one(SushiDSP::host::AudioImportQueue& queue,
                      std::vector<SushiDSP::host::AudioImport>& out)
    {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + K_PATIENCE;
        while (std::chrono::steady_clock::now() < deadline)
        {
            queue.drain(out);
            if (!out.empty())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }
} // namespace

TEST(Medium_AudioImportQueue, AudioImportQueueStartsIdle)
{
    SushiDSP::host::AudioImportQueue queue{SushiDSP::host::AudioImportCache(
        (std::filesystem::temp_directory_path() / "sushidsp_import_idle").string())};

    std::vector<SushiDSP::host::AudioImport> drained;
    queue.drain(drained);

    EXPECT_TRUE(drained.empty());
    EXPECT_EQ(queue.pending(), 0u);
    EXPECT_TRUE(queue.decoding().empty());
}

TEST(Medium_AudioImportQueue, AudioImportQueueReportsAFailureAndKeepsThePlacementItWasGiven)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_import_queue";
    std::filesystem::remove_all(root);

    SushiDSP::host::AudioImportQueue queue{SushiDSP::host::AudioImportCache(root.string())};
    const std::string missing = (root / "nothing_here.mp3").string();
    queue.request(missing, 5, 987654, 48000.0);

    std::vector<SushiDSP::host::AudioImport> drained;
    ASSERT_TRUE(wait_for_one(queue, drained));
    ASSERT_EQ(drained.size(), 1u);

    EXPECT_EQ(drained[0].source_path, missing);
    EXPECT_EQ(drained[0].track, 5u);
    EXPECT_EQ(drained[0].start_frame, 987654u);
    EXPECT_TRUE(drained[0].wav_path.empty());
    EXPECT_FALSE(drained[0].error.empty());
}

TEST(Medium_AudioImportQueue, AudioImportQueueHandsBackOneResultPerRequest)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_import_queue_many";
    std::filesystem::remove_all(root);

    SushiDSP::host::AudioImportQueue queue{SushiDSP::host::AudioImportCache(root.string())};
    queue.request((root / "one.mp3").string(), 0, 0, 48000.0);
    queue.request((root / "two.mp3").string(), 1, 64, 48000.0);
    queue.request((root / "three.mp3").string(), 2, 128, 48000.0);

    std::vector<SushiDSP::host::AudioImport> drained;
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + K_PATIENCE;
    while (drained.size() < 3u && std::chrono::steady_clock::now() < deadline)
    {
        queue.drain(drained);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(drained[0].track, 0u);
    EXPECT_EQ(drained[1].track, 1u);
    EXPECT_EQ(drained[2].track, 2u);
    EXPECT_EQ(queue.pending(), 0u);
}
