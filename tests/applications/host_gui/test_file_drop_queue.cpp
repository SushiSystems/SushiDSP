/**************************************************************************/
/* test_file_drop_queue.cpp                                               */
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
 * @file test_file_drop_queue.cpp
 * @brief Tests for FileDropQueue.
 * @author Mustafa Garip & Sushi Systems
 */

#include "file_drop_queue.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <thread>
#include <vector>

using SushiDSP::host::FileDrop;
using SushiDSP::host::FileDropQueue;

namespace
{
    constexpr std::size_t K_PATH_COUNT = 1000;

    /// @brief Returns the paths of a list of drops, in order.
    std::vector<std::string> paths_of(const std::vector<FileDrop>& drops)
    {
        std::vector<std::string> paths;
        for (const FileDrop& drop : drops)
        {
            paths.push_back(drop.path);
        }
        return paths;
    }

    /// @brief Returns the paths "0.wav" up to the one before count, in order.
    std::vector<std::string> numbered_paths(std::size_t count)
    {
        std::vector<std::string> paths;
        for (std::size_t index = 0; index < count; ++index)
        {
            paths.push_back(std::to_string(index) + ".wav");
        }
        return paths;
    }
} // namespace

TEST(Small_FileDropQueue, FileDropQueueDrainsInTheOrderPushed)
{
    FileDropQueue queue;
    queue.push("first.wav", 1.0, 2.0);
    queue.push("second.wav", 3.0, 4.0);
    queue.push("third.wav", 5.0, 6.0);

    std::vector<FileDrop> drained;
    queue.drain(drained);

    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(paths_of(drained), (std::vector<std::string>{"first.wav", "second.wav", "third.wav"}));
    EXPECT_EQ(drained[0].screen_x, 1.0);
    EXPECT_EQ(drained[0].screen_y, 2.0);
    EXPECT_EQ(drained[2].screen_x, 5.0);
    EXPECT_EQ(drained[2].screen_y, 6.0);
}

TEST(Small_FileDropQueue, FileDropQueueIsEmptyAfterADrain)
{
    FileDropQueue queue;
    queue.push("only.wav", 0.0, 0.0);
    EXPECT_FALSE(queue.empty());

    std::vector<FileDrop> drained;
    queue.drain(drained);

    EXPECT_TRUE(queue.empty());

    std::vector<FileDrop> second_drain;
    queue.drain(second_drain);
    EXPECT_TRUE(second_drain.empty());
}

TEST(Small_FileDropQueue, FileDropQueueDrainAppendsToACallerSVector)
{
    FileDropQueue queue;
    queue.push("dropped.wav", 10.0, 20.0);
    std::vector<FileDrop> drained;
    drained.push_back(FileDrop{"already_there.wav", -1.0, -1.0});

    queue.drain(drained);

    EXPECT_EQ(paths_of(drained), (std::vector<std::string>{"already_there.wav", "dropped.wav"}));
}

TEST(Medium_FileDropQueue, FileDropQueueKeepsEveryDropAPusherThreadMade)
{
    FileDropQueue queue;
    std::thread pusher(
        [&queue]()
        {
            for (std::size_t index = 0; index < K_PATH_COUNT; ++index)
            {
                queue.push(std::to_string(index) + ".wav", static_cast<double>(index), static_cast<double>(index));
            }
        });
    pusher.join();

    std::vector<FileDrop> drained;
    queue.drain(drained);

    EXPECT_EQ(paths_of(drained), numbered_paths(K_PATH_COUNT));
}
