/**************************************************************************/
/* test_async_log_sink.cpp                                                */
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
 * @file test_async_log_sink.cpp
 * @brief Tests the asynchronous sink: order, flush, the drain on destruction and a full queue.
 * @author Mustafa Garip
 */

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/log/AsyncLogSink.hpp"
#include "SushiDSP/log/CapturingLogSink.hpp"

namespace
{
    /// @brief Names the lines this file writes.
    constexpr SushiDSP::LogCategory K_ASYNC_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category("async_test");

    /// @brief Writes one numbered line to a sink.
    void write_line(SushiDSP::ILogSink& sink, std::size_t index)
    {
        const std::string text = "line " + std::to_string(index);
        sink.write(SushiDSP::LogRecord{ SushiDSP::LogLevel::Info, K_ASYNC_TEST_LOG_CATEGORY, text });
    }

    /// @brief Holds every write() until open() is called, then passes the lines on.
    class GatedLogSink final : public SushiDSP::ILogSink
    {
    public:
        /// @brief Starts closed, passing lines on to a sink once opened.
        explicit GatedLogSink(std::shared_ptr<SushiDSP::ILogSink> downstream)
            : downstream_(std::move(downstream))
        {
        }

        /// @brief Blocks until the gate opens, then passes the line on.
        void write(const SushiDSP::LogRecord& record) noexcept override
        {
            entered_.store(true, std::memory_order_release);
            {
                std::unique_lock<std::mutex> lock(mutex_);
                opened_.wait(lock, [this] { return open_; });
            }
            downstream_->write(record);
        }

        /// @brief Passes the flush on.
        void flush() noexcept override
        {
            downstream_->flush();
        }

        /// @brief Reports whether a write() has reached the gate.
        bool has_entered() const noexcept
        {
            return entered_.load(std::memory_order_acquire);
        }

        /// @brief Lets every held and later write() through.
        void open()
        {
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                open_ = true;
            }
            opened_.notify_all();
        }

    private:
        /// @brief Receives the lines once the gate is open.
        std::shared_ptr<SushiDSP::ILogSink> downstream_;

        /// @brief Guards open_.
        std::mutex mutex_;

        /// @brief Wakes the writes held at the gate.
        std::condition_variable opened_;

        /// @brief Tells whether the gate is open.
        bool open_ = false;

        /// @brief Tells whether a write() has reached the gate.
        std::atomic<bool> entered_{ false };
    };
}

TEST(Unit_Log, AsyncSinkDeliversEveryLineInOrderBeforeFlushReturns)
{
    constexpr std::size_t kLines = 500;

    const auto             capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::AsyncLogSink sink(capture, 1024);

    for (std::size_t index = 0; index < kLines; ++index)
    {
        write_line(sink, index);
    }
    sink.flush();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), kLines);
    for (std::size_t index = 0; index < kLines; ++index)
    {
        EXPECT_EQ(lines[index].message, "line " + std::to_string(index));
        EXPECT_EQ(lines[index].category, "async_test");
        EXPECT_EQ(lines[index].level, SushiDSP::LogLevel::Info);
    }
    EXPECT_EQ(sink.dropped_count(), 0u);
    EXPECT_GE(capture->flush_count(), 1u);
}

TEST(Unit_Log, AsyncSinkFlushReturnsWhenNothingIsQueued)
{
    const auto             capture = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::AsyncLogSink sink(capture, 16);

    sink.flush();
    sink.flush();

    EXPECT_TRUE(capture->lines().empty());
    EXPECT_GE(capture->flush_count(), 1u);
}

TEST(Unit_Log, AsyncSinkDeliversWhatIsQueuedWhenItIsDestroyed)
{
    constexpr std::size_t kLines = 100;

    const auto capture = std::make_shared<SushiDSP::CapturingLogSink>();
    {
        SushiDSP::AsyncLogSink sink(capture, 128);
        for (std::size_t index = 0; index < kLines; ++index)
        {
            write_line(sink, index);
        }
    }

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), kLines);
    EXPECT_EQ(lines.front().message, "line 0");
    EXPECT_EQ(lines.back().message, "line 99");
    EXPECT_GE(capture->flush_count(), 1u);
}

TEST(Unit_Log, AsyncSinkDropsAndCountsWhenTheQueueIsFullInsteadOfBlocking)
{
    const auto             capture = std::make_shared<SushiDSP::CapturingLogSink>();
    const auto             gate = std::make_shared<GatedLogSink>(capture);
    SushiDSP::AsyncLogSink sink(gate, 2);

    write_line(sink, 0);
    // The worker holds line 0 at the gate, so the queue of two is empty and nothing drains it.
    while (!gate->has_entered())
    {
        std::this_thread::yield();
    }

    write_line(sink, 1);
    write_line(sink, 2);
    EXPECT_EQ(sink.dropped_count(), 0u);
    write_line(sink, 3);
    write_line(sink, 4);
    write_line(sink, 5);
    EXPECT_EQ(sink.dropped_count(), 3u);

    gate->open();
    sink.flush();

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0].message, "line 0");
    EXPECT_EQ(lines[1].message, "line 1");
    EXPECT_EQ(lines[2].message, "line 2");
    EXPECT_EQ(lines[3].level, SushiDSP::LogLevel::Warn);
    EXPECT_EQ(lines[3].category, "log");
    EXPECT_NE(lines[3].message.find("3 lines were dropped"), std::string::npos)
        << lines[3].message;
}
