/**************************************************************************/
/* logger.hpp                                                             */
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
 * @file logger.hpp
 * @brief Declares the logger: the levels of every category, the sink and how records reach it.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

#include <SushiDSP/foundation/log/log_delivery.hpp>
#include <SushiDSP/foundation/log/log_formatter.hpp>
#include <SushiDSP/foundation/log/log_level_table.hpp>
#include <SushiDSP/foundation/log/log_record_sink.hpp>
#include <SushiDSP/foundation/log/log_sink.hpp>
#include <SushiDSP/foundation/log/log_site.hpp>

namespace SushiDSP
{
    /// @brief Delivers records from the logger thread; see async_log_sink.hpp.
    class AsyncLogSink;

    /// @brief Formats log calls into record lines and hands each one that passes to one sink.
    /// Never call it from the audio thread; see modules/foundation/log/README.md.
    class Logger final
    {
        public:
            /// @brief Starts with a sink, a level for every category and a delivery.
            /// @param sink Null means a StderrLogSink.
            /// @param environment_variable Read at the first log call when not empty, such as
            /// `SD_LOG_LEVEL`; see log_level_setting.hpp for what it holds.
            Logger(std::shared_ptr<LogRecordSink> sink, LogLevel every, LogDelivery delivery,
                   std::string_view environment_variable = {});

            /// @brief Delivers what is queued and stops the logger thread.
            ~Logger();

            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;
            Logger(Logger&&) = delete;
            Logger& operator=(Logger&&) = delete;

            /// @brief Replaces the sink; records already handed to the old one stay with it.
            /// @param sink Null restores the StderrLogSink.
            /// @return The sink that was in place.
            std::shared_ptr<LogRecordSink> set_sink(std::shared_ptr<LogRecordSink> sink) noexcept;

            /// @brief Replaces the sink with a callable; an empty one restores the StderrLogSink.
            void set_log_sink(LogSink sink);

            /// @brief Sets every category's level; made before the first log call, it wins over
            /// the environment.
            void set_log_level(LogLevel level) noexcept;

            /// @brief Sets one category's level; made before the first log call, it wins over
            /// the environment for that category.
            void set_log_level(const LogCategory& category, LogLevel level) noexcept;

            /// @brief Returns the level a category passes records at.
            [[nodiscard]] LogLevel log_level(const LogCategory& category) const noexcept;

            /// @brief Returns the level of every category that has no level of its own.
            [[nodiscard]] LogLevel log_level() const noexcept;

            /// @brief Reports whether a record of this level and category would reach the sink.
            /// The first call reads the environment variable.
            [[nodiscard]] bool is_enabled(LogLevel level, const LogCategory& category) noexcept;

            /// @brief Chooses the thread records reach the sink on; switching to Synchronous
            /// first waits for every queued record.
            void set_delivery(LogDelivery delivery) noexcept;

            /// @brief Returns the thread records reach the sink on.
            [[nodiscard]] LogDelivery delivery() const noexcept;

            /// @brief Formats a message on the stack and writes it when its level passes.
            /// @param format Positional `{}` markers only; see log_formatter.hpp.
            template <typename... Values>
            void log(LogLevel level, const LogCategory& category, LogSite site,
                     std::string_view format, const Values&... values) noexcept
            {
                if (!is_enabled(level, category))
                    return;
                char message[K_MAX_LOG_MESSAGE_LENGTH];
                const std::size_t length =
                    format_log_message(message, sizeof(message), format, values...);
                write(level, category, site, std::string_view(message, length));
            }

            /// @brief Enters another logger's record, past every level gate, as if logged here.
            /// The record keeps its time; this logger numbers the thread and writes the line.
            void forward(const LogRecord& record) noexcept;

            /// @brief Writes a formatted message as one record line, whatever its level.
            /// A Fatal record is delivered on the calling thread after the queue, then flushed.
            void write(LogLevel level, const LogCategory& category, LogSite site,
                       std::string_view message) noexcept;

            /// @brief Returns once every earlier record has reached the sink and it is flushed.
            void flush() noexcept;

            /// @brief Delivers what is queued, stops the logger thread and delivers on the caller
            /// from then on.
            void shutdown() noexcept;

            /// @brief Returns how many records a full queue has refused.
            [[nodiscard]] std::uint64_t dropped_count() const noexcept;

        private:
            /// @brief Forwards the logger thread's records to whatever sink is in place.
            class CurrentSink;

            /// @brief Reads the environment variable into the levels, once.
            void apply_environment() noexcept;

            /// @brief Writes the record line and delivers the record.
            void deliver_line(LogLevel level, std::string_view project, std::string_view module,
                              std::string_view file, std::uint32_t line, std::int64_t time_ns,
                              std::string_view message) noexcept;

            /// @brief Hands a record to the sink on the thread the delivery names.
            void deliver(const LogRecord& record) noexcept;

            /// @brief Writes a record to the sink in place, catching what the sink throws.
            void write_to_sink(const LogRecord& record) noexcept;

            /// @brief Flushes the sink in place.
            void flush_sink() noexcept;

            /// @brief Returns the logger thread's sink, starting it on the first call.
            AsyncLogSink& async_sink();

            /// @brief Returns the sink in place, kept alive for the caller's use.
            std::shared_ptr<LogRecordSink> current_sink() const noexcept;

            /// @brief Holds the level of every category.
            LogLevelTable levels_;

            /// @brief Names the environment variable read at the first log call; may be empty.
            std::string_view environment_variable_;

            /// @brief Turns true once the environment has been read.
            std::atomic<bool> environment_read_{false};

            /// @brief Runs apply_environment() once.
            std::once_flag environment_once_;

            /// @brief Turns true when code sets every category, so the environment is ignored.
            std::atomic<bool> every_set_in_code_{false};

            /// @brief Holds the sink put back when a null sink is set.
            std::shared_ptr<LogRecordSink> stderr_sink_;

            /// @brief Guards sink_; held only while the pointer is copied or swapped.
            mutable std::mutex sink_mutex_;

            /// @brief Holds the sink every record goes to.
            std::shared_ptr<LogRecordSink> sink_;

            /// @brief Holds the thread records reach the sink on.
            std::atomic<LogDelivery> delivery_;

            /// @brief Starts the logger thread once.
            std::once_flag async_once_;

            /// @brief Owns the logger thread and its queue, once started.
            std::unique_ptr<AsyncLogSink> async_;

            /// @brief Points at async_ once it is started; null before.
            std::atomic<AsyncLogSink*> async_started_{nullptr};
    };
} // namespace SushiDSP
