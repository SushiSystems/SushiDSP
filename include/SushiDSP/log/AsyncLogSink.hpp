/**************************************************************************/
/* AsyncLogSink.hpp                                                       */
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
 * @file AsyncLogSink.hpp
 * @brief Declares the sink that hands lines to a worker thread through a bounded queue.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "SushiDSP/log/ILogSink.hpp"
#include "SushiDSP/log/LogLineQueue.hpp"

namespace SushiDSP
{
    /// @brief Holds the queue length an application gets when it has no reason to choose one.
    inline constexpr std::size_t kDefaultLogQueueCapacity = 256;

    /// @brief Queues each line and lets a worker thread write it to another sink, in order.
    /// write() neither blocks nor allocates: a line that finds the queue full is dropped and
    /// counted, and the worker reports the count downstream. See include/SushiDSP/README.md.
    class AsyncLogSink final : public ILogSink
    {
    public:
        /// @brief Allocates the queue and starts the worker thread.
        /// @param downstream Receives every line on the worker thread; must not be null.
        /// @param queue_capacity Lines the queue holds; rounded up to a power of two.
        AsyncLogSink(std::shared_ptr<ILogSink> downstream, std::size_t queue_capacity);

        /// @brief Writes every queued line downstream, flushes it and joins the worker.
        ~AsyncLogSink() override;

        AsyncLogSink(const AsyncLogSink&) = delete;
        AsyncLogSink& operator=(const AsyncLogSink&) = delete;
        AsyncLogSink(AsyncLogSink&&) = delete;
        AsyncLogSink& operator=(AsyncLogSink&&) = delete;

        /// @brief Queues one line, or drops and counts it when the queue is full.
        void write(const LogRecord& record) noexcept override;

        /// @brief Blocks until every line queued before the call is written and flushed downstream.
        /// @pre The caller is not the worker thread.
        void flush() noexcept override;

        /// @brief Returns how many lines a full queue has refused.
        std::uint64_t dropped_count() const noexcept;

    private:
        /// @brief Runs on the worker thread until the destructor stops it.
        void run() noexcept;

        /// @brief Writes every line now in the queue downstream.
        void drain() noexcept;

        /// @brief Writes one line naming the drops counted since the last such line.
        void report_drops() noexcept;

        /// @brief Reports whether the worker has a reason to leave its wait.
        /// @pre wake_mutex_ is held.
        bool has_work() const noexcept;

        /// @brief Receives the lines on the worker thread.
        std::shared_ptr<ILogSink> downstream_;

        /// @brief Carries the lines from the callers to the worker.
        LogLineQueue queue_;

        /// @brief Holds the line the worker is writing; only the worker touches it.
        QueuedLogLine scratch_;

        /// @brief Counts the lines the queue accepted.
        std::atomic<std::uint64_t> accepted_{ 0 };

        /// @brief Counts the lines the worker took out of the queue.
        std::atomic<std::uint64_t> taken_{ 0 };

        /// @brief Counts the lines a full queue refused.
        std::atomic<std::uint64_t> dropped_{ 0 };

        /// @brief Counts the drops already reported downstream; only the worker touches it.
        std::uint64_t reported_drops_ = 0;

        /// @brief Guards flush_requests_, flushes_served_ and running_.
        mutable std::mutex wake_mutex_;

        /// @brief Wakes the worker when a line, a flush request or the stop arrives.
        std::condition_variable wake_;

        /// @brief Wakes flush() callers when the worker has served their request.
        std::condition_variable flushed_;

        /// @brief Counts the flush() calls made.
        std::uint64_t flush_requests_ = 0;

        /// @brief Counts the flush() calls the worker has served.
        std::uint64_t flushes_served_ = 0;

        /// @brief Stays true until the destructor asks the worker to stop.
        bool running_ = true;

        /// @brief Runs run(); declared last so every field above exists before it starts.
        std::thread worker_;
    };
}
