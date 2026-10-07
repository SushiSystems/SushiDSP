/**************************************************************************/
/* test_log_line_queue.cpp                                                */
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
 * @file test_log_line_queue.cpp
 * @brief Tests the bounded log line queue: order, capacity, refusal when full, several producers.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/log/LogLineQueue.hpp"

namespace
{
    /// @brief Names the lines this file queues.
    inline constexpr SushiDSP::LogCategory K_QUEUE_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category("queue_test");

    /// @brief Returns a record that carries a text; the text must outlive the record.
    SushiDSP::LogRecord record_of(const std::string& text)
    {
        return SushiDSP::LogRecord{ SushiDSP::LogLevel::Info, K_QUEUE_TEST_LOG_CATEGORY, text };
    }

    /// @brief Returns the text of a queued line.
    std::string text_of(const SushiDSP::QueuedLogLine& line)
    {
        return std::string(line.record().message);
    }
}

TEST(Unit_Log, LogLineQueueRoundsItsCapacityUpToAPowerOfTwo)
{
    EXPECT_EQ(SushiDSP::LogLineQueue(0).capacity(), 2u);
    EXPECT_EQ(SushiDSP::LogLineQueue(3).capacity(), 4u);
    EXPECT_EQ(SushiDSP::LogLineQueue(256).capacity(), 256u);
}

TEST(Unit_Log, LogLineQueuePopsLinesInPushOrderWithTheirLevelAndCategory)
{
    SushiDSP::LogLineQueue queue(4);
    const auto             line = std::make_unique<SushiDSP::QueuedLogLine>();

    EXPECT_FALSE(queue.pop(*line));
    ASSERT_TRUE(queue.push(record_of("first")));
    ASSERT_TRUE(queue.push(SushiDSP::LogRecord{ SushiDSP::LogLevel::Error,
                                                K_QUEUE_TEST_LOG_CATEGORY, "second" }));

    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "first");
    EXPECT_EQ(line->level, SushiDSP::LogLevel::Info);
    EXPECT_EQ(line->category.name(), "queue_test");
    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "second");
    EXPECT_EQ(line->level, SushiDSP::LogLevel::Error);
    EXPECT_FALSE(queue.pop(*line));
}

TEST(Unit_Log, LogLineQueueRefusesALineWhenFullAndTakesOneAgainAfterAPop)
{
    SushiDSP::LogLineQueue queue(2);
    const auto             line = std::make_unique<SushiDSP::QueuedLogLine>();

    ASSERT_TRUE(queue.push(record_of("a")));
    ASSERT_TRUE(queue.push(record_of("b")));
    EXPECT_FALSE(queue.push(record_of("c")));

    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "a");
    EXPECT_TRUE(queue.push(record_of("d")));

    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "b");
    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "d");
}

TEST(Unit_Log, LogLineQueueKeepsEveryLineOfSeveralProducersInEachProducersOrder)
{
    constexpr std::size_t kProducers = 4;
    constexpr std::size_t kLinesEach = 2000;

    SushiDSP::LogLineQueue   queue(64);
    std::vector<std::thread> producers;
    for (std::size_t producer = 0; producer < kProducers; ++producer)
    {
        producers.emplace_back([&queue, producer]
        {
            for (std::size_t index = 0; index < kLinesEach; ++index)
            {
                const std::string text = std::to_string(producer) + ":" + std::to_string(index);
                while (!queue.push(record_of(text)))
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    const auto               line = std::make_unique<SushiDSP::QueuedLogLine>();
    std::vector<std::string> received;
    received.reserve(kProducers * kLinesEach);
    while (received.size() < kProducers * kLinesEach)
    {
        if (queue.pop(*line))
        {
            received.push_back(text_of(*line));
        }
        else
        {
            std::this_thread::yield();
        }
    }
    for (std::thread& producer : producers)
    {
        producer.join();
    }
    EXPECT_FALSE(queue.pop(*line));

    std::vector<std::size_t> next_index(kProducers, 0);
    for (const std::string& text : received)
    {
        const std::size_t colon = text.find(':');
        ASSERT_NE(colon, std::string::npos);
        const std::size_t producer = std::stoul(text.substr(0, colon));
        const std::size_t index = std::stoul(text.substr(colon + 1));
        ASSERT_LT(producer, kProducers);
        EXPECT_EQ(index, next_index[producer]);
        next_index[producer] = index + 1;
    }
}
