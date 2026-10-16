/**************************************************************************/
/* async_log_sink.cpp                                                     */
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
 * @file async_log_sink.cpp
 * @brief Implements the asynchronous sink's queueing, its worker loop and its flush.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/async_log_sink.hpp>

#include <chrono>
#include <utility>

#include <SushiDSP/foundation/log/log_formatter.hpp>
#include <SushiDSP/foundation/log/log_line.hpp>
#include <SushiDSP/foundation/log/log_log_category.hpp>
#include <SushiDSP/foundation/log/log_site.hpp>
#include <SushiDSP/foundation/log/log_thread.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Bounds how long a line waits when its wake-up races the worker going to sleep.
        constexpr std::chrono::milliseconds K_IDLE_WAIT{50};

        /// @brief Sizes the drop report; its text and two 20-digit counts fit.
        constexpr std::size_t K_DROP_REPORT_LENGTH = 128;
    } // namespace

    AsyncLogSink::AsyncLogSink(std::shared_ptr<LogRecordSink> downstream, std::size_t queue_capacity)
        : downstream_(std::move(downstream)), queue_(queue_capacity),
          worker_(&AsyncLogSink::run, this)
    {
    }

    AsyncLogSink::~AsyncLogSink()
    {
        stop();
    }

    void AsyncLogSink::stop() noexcept
    {
        {
            const std::lock_guard<std::mutex> lock(wake_mutex_);
            running_ = false;
        }
        wake_.notify_one();
        if (worker_.joinable())
            worker_.join();
    }

    void AsyncLogSink::write(const LogRecord& record) noexcept
    {
        if (!queue_.push(record))
        {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        accepted_.fetch_add(1, std::memory_order_release);
        // No lock is taken here, so a wake-up can be missed; K_IDLE_WAIT bounds the delay.
        wake_.notify_one();
    }

    void AsyncLogSink::flush() noexcept
    {
        std::unique_lock<std::mutex> lock(wake_mutex_);
        const std::uint64_t request = ++flush_requests_;
        wake_.notify_one();
        flushed_.wait(lock,
                      [this, request]
                      {
                          return flushes_served_ >= request;
                      });
    }

    std::uint64_t AsyncLogSink::dropped_count() const noexcept
    {
        return dropped_.load(std::memory_order_relaxed);
    }

    void AsyncLogSink::run() noexcept
    {
        for (;;)
        {
            std::uint64_t requested = 0;
            bool flush_wanted = false;
            bool stopping = false;
            {
                std::unique_lock<std::mutex> lock(wake_mutex_);
                wake_.wait_for(lock, K_IDLE_WAIT,
                               [this]
                               {
                                   return has_work();
                               });
                requested = flush_requests_;
                flush_wanted = requested != flushes_served_;
                stopping = !running_;
            }

            // The request count was read before the drain, so the drain covers every line it promises.
            drain();

            if (flush_wanted || stopping)
            {
                downstream_->flush();
                {
                    const std::lock_guard<std::mutex> lock(wake_mutex_);
                    flushes_served_ = requested;
                }
                flushed_.notify_all();
            }
            if (stopping)
            {
                return;
            }
        }
    }

    void AsyncLogSink::drain() noexcept
    {
        while (queue_.pop(scratch_))
        {
            taken_.fetch_add(1, std::memory_order_release);
            report_drops();
            downstream_->write(scratch_.record());
        }
    }

    void AsyncLogSink::report_drops() noexcept
    {
        const std::uint64_t dropped = dropped_.load(std::memory_order_relaxed);
        if (dropped == reported_drops_)
        {
            return;
        }

        char text[K_DROP_REPORT_LENGTH];
        const std::size_t length = format_log_message(
            text, sizeof(text),
            "the log queue of {} lines was full; {} lines were dropped and are not written",
            queue_.capacity(), dropped - reported_drops_);
        reported_drops_ = dropped;
        const std::string_view message(text, length);
        const std::string_view file = log_site_file_name(__FILE__);
        const std::uint32_t at = __LINE__;

        const auto now = std::chrono::system_clock::now();
        const std::uint32_t thread = current_log_thread_number();
        char line[K_MAX_LOG_PREFIX_LENGTH + K_DROP_REPORT_LENGTH];
        const std::size_t line_length = format_log_line(
            line, sizeof(line),
            LogLineFields{now, LogLevel::Warn, K_LOG_LOG_CATEGORY.project().name(),
                          K_LOG_LOG_CATEGORY.module(), thread, file, at, message});
        const std::string_view whole(line, line_length);
        downstream_->write(LogRecord{
            LogLevel::Warn, K_LOG_LOG_CATEGORY.project().name(), K_LOG_LOG_CATEGORY.module(),
            file, at, thread,
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
            whole.substr(line_length - message.size()), whole});
    }
    bool AsyncLogSink::has_work() const noexcept
    {
        return !running_ || flush_requests_ != flushes_served_ ||
               accepted_.load(std::memory_order_acquire) != taken_.load(std::memory_order_acquire);
    }
} // namespace SushiDSP
