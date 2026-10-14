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
 * @file test_file_drop_queue.cpp
 * @brief Tests for FileDropQueue.
 * @author Mustafa Garip & Sushi Systems
 */

#include "file_drop_queue.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <thread>

using SushiDSP::host::FileDrop;
using SushiDSP::host::FileDropQueue;

TEST(FileDropQueue, FileDropQueueDrainsInTheOrderPushed)
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

TEST(FileDropQueue, FileDropQueueIsEmptyAfterADrain)
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

TEST(FileDropQueue, FileDropQueueDrainAppendsToACallerSVector)
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

TEST(FileDropQueue, FileDropQueueSurvivesConcurrentPushAndDrain)
{
    static constexpr int K_PATH_COUNT = 1000;
    FileDropQueue queue;

    std::thread pusher(
        [&queue]()
        {
            for (int i = 0; i < K_PATH_COUNT; ++i)
            {
                queue.push(std::to_string(i) + ".wav", static_cast<double>(i),
                           static_cast<double>(i));
            }
        });

    std::vector<FileDrop> drained;
    while (drained.size() < static_cast<std::size_t>(K_PATH_COUNT))
    {
        queue.drain(drained);
    }

    pusher.join();

    std::vector<FileDrop> trailing;
    queue.drain(trailing);
    drained.insert(drained.end(), trailing.begin(), trailing.end());

    ASSERT_EQ(drained.size(), static_cast<std::size_t>(K_PATH_COUNT));

    std::set<std::string> seen;
    for (const FileDrop& drop : drained)
    {
        EXPECT_TRUE(seen.insert(drop.path).second) << "duplicate path: " << drop.path;
    }
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(K_PATH_COUNT));
}
