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

#include <SushiDSP/foundation/log/log_line_queue.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

namespace
{
    /// @brief Names the lines this file queues.
    inline constexpr SushiDSP::LogCategory K_QUEUE_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "queue_test");

    /// @brief Returns a record that carries a text; the text must outlive the record.
    SushiDSP::LogRecord record_of(const std::string& text)
    {
        return SushiDSP::LogRecord{SushiDSP::LogLevel::Info, "sushidsp", "queue_test", "a.cpp", 1, 1, 0,
                                   text, text};
    }

    /// @brief Returns the text of a queued line.
    std::string text_of(const SushiDSP::QueuedLogLine& line)
    {
        return std::string(line.record().message);
    }
} // namespace

TEST(Medium_LogLineQueue, LogLineQueueRoundsItsCapacityUpToAPowerOfTwo)
{
    EXPECT_EQ(SushiDSP::LogLineQueue(0).capacity(), 2u);
    EXPECT_EQ(SushiDSP::LogLineQueue(3).capacity(), 4u);
    EXPECT_EQ(SushiDSP::LogLineQueue(256).capacity(), 256u);
}

TEST(Medium_LogLineQueue, LogLineQueuePopsLinesInPushOrderWithTheirLevelAndCategory)
{
    SushiDSP::LogLineQueue queue(4);
    const auto line = std::make_unique<SushiDSP::QueuedLogLine>();

    EXPECT_FALSE(queue.pop(*line));
    ASSERT_TRUE(queue.push(record_of("first")));
    ASSERT_TRUE(queue.push(
        SushiDSP::LogRecord{SushiDSP::LogLevel::Error, "sushidsp", "queue_test", "a.cpp", 1, 1,
                            0, "second", "second"}));

    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "first");
    EXPECT_EQ(line->level, SushiDSP::LogLevel::Info);
    EXPECT_EQ(line->record().module, "queue_test");
    ASSERT_TRUE(queue.pop(*line));
    EXPECT_EQ(text_of(*line), "second");
    EXPECT_EQ(line->level, SushiDSP::LogLevel::Error);
    EXPECT_FALSE(queue.pop(*line));
}

TEST(Medium_LogLineQueue, LogLineQueueRefusesALineWhenFullAndTakesOneAgainAfterAPop)
{
    SushiDSP::LogLineQueue queue(2);
    const auto line = std::make_unique<SushiDSP::QueuedLogLine>();

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

TEST(Medium_LogLineQueue, LogLineQueueKeepsEveryLineOfSeveralProducersInEachProducersOrder)
{
    constexpr std::size_t K_PRODUCERS = 4;
    static constexpr std::size_t K_LINES_EACH = 2000;

    SushiDSP::LogLineQueue queue(64);
    std::vector<std::thread> producers;
    for (std::size_t producer = 0; producer < K_PRODUCERS; ++producer)
    {
        producers.emplace_back(
            [&queue, producer]
            {
                for (std::size_t index = 0; index < K_LINES_EACH; ++index)
                {
                    const std::string text = std::to_string(producer) + ":" + std::to_string(index);
                    while (!queue.push(record_of(text)))
                    {
                        std::this_thread::yield();
                    }
                }
            });
    }

    const auto line = std::make_unique<SushiDSP::QueuedLogLine>();
    std::vector<std::string> received;
    received.reserve(K_PRODUCERS * K_LINES_EACH);
    while (received.size() < K_PRODUCERS * K_LINES_EACH)
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

    std::vector<std::size_t> next_index(K_PRODUCERS, 0);
    for (const std::string& text : received)
    {
        const std::size_t colon = text.find(':');
        ASSERT_NE(colon, std::string::npos);
        const std::size_t producer = std::stoul(text.substr(0, colon));
        const std::size_t index = std::stoul(text.substr(colon + 1));
        ASSERT_LT(producer, K_PRODUCERS);
        EXPECT_EQ(index, next_index[producer]);
        next_index[producer] = index + 1;
    }
}

TEST(Medium_LogLineQueue, LogLineQueueCarriesAWholeLineAndFindsTheMessageAtItsEnd)
{
    SushiDSP::LogLineQueue queue(2);
    const auto line = std::make_unique<SushiDSP::QueuedLogLine>();
    const std::string whole = "[12:00:00.000] [INFO] [queue_test] [t1] [a.cpp:1] ready";

    ASSERT_TRUE(queue.push(SushiDSP::LogRecord{SushiDSP::LogLevel::Info, "sushidsp", "queue_test",
                                               "a.cpp", 1, 1, 7,
                                               std::string_view(whole).substr(whole.size() - 5),
                                               whole}));
    ASSERT_TRUE(queue.pop(*line));

    const SushiDSP::LogRecord record = line->record();
    EXPECT_EQ(record.text, whole);
    EXPECT_EQ(record.project, "sushidsp");
    EXPECT_EQ(record.file, "a.cpp");
    EXPECT_EQ(record.time_ns, 7);
    EXPECT_EQ(record.message, "ready");
}