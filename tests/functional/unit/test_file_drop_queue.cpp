/**************************************************************************/
/* test_file_drop_queue.cpp                                               */
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

/**
 * @file test_file_drop_queue.cpp
 * @brief Tests for FileDropQueue.
 * @author Mustafa Garip & Sushi Systems
 */

#include "FileDropQueue.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <thread>

using SushiDSP::host::FileDrop;
using SushiDSP::host::FileDropQueue;

TEST(Unit_Dsp, FileDropQueueDrainsInTheOrderPushed)
{
    FileDropQueue queue;
    queue.push("first.wav", 1.0, 2.0);
    queue.push("second.wav", 3.0, 4.0);
    queue.push("third.wav", 5.0, 6.0);

    std::vector<FileDrop> drained;
    queue.drain(drained);

    ASSERT_EQ(drained.size(), 3u);
    EXPECT_EQ(drained[0].path, "first.wav");
    EXPECT_EQ(drained[1].path, "second.wav");
    EXPECT_EQ(drained[2].path, "third.wav");
    EXPECT_DOUBLE_EQ(drained[0].screen_x, 1.0);
    EXPECT_DOUBLE_EQ(drained[0].screen_y, 2.0);
    EXPECT_DOUBLE_EQ(drained[2].screen_x, 5.0);
    EXPECT_DOUBLE_EQ(drained[2].screen_y, 6.0);
}

TEST(Unit_Dsp, FileDropQueueIsEmptyAfterADrain)
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

TEST(Unit_Dsp, FileDropQueueDrainAppendsToACallerSVector)
{
    FileDropQueue queue;
    queue.push("dropped.wav", 10.0, 20.0);

    std::vector<FileDrop> drained;
    drained.push_back(FileDrop{"already_there.wav", -1.0, -1.0});
    queue.drain(drained);

    ASSERT_EQ(drained.size(), 2u);
    EXPECT_EQ(drained[0].path, "already_there.wav");
    EXPECT_EQ(drained[1].path, "dropped.wav");
}

TEST(Unit_Dsp, FileDropQueueSurvivesConcurrentPushAndDrain)
{
    constexpr int kPathCount = 1000;
    FileDropQueue queue;

    std::thread pusher([&queue]()
    {
        for (int i = 0; i < kPathCount; ++i)
        {
            queue.push(std::to_string(i) + ".wav", static_cast<double>(i), static_cast<double>(i));
        }
    });

    std::vector<FileDrop> drained;
    while (drained.size() < static_cast<std::size_t>(kPathCount))
    {
        queue.drain(drained);
    }

    pusher.join();

    std::vector<FileDrop> trailing;
    queue.drain(trailing);
    drained.insert(drained.end(), trailing.begin(), trailing.end());

    ASSERT_EQ(drained.size(), static_cast<std::size_t>(kPathCount));

    std::set<std::string> seen;
    for (const FileDrop& drop : drained)
    {
        EXPECT_TRUE(seen.insert(drop.path).second) << "duplicate path: " << drop.path;
    }
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(kPathCount));
}
