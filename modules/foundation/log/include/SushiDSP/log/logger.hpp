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
 * @brief Declares the logger: the owner of the current sink and the runtime minimum level.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>

#include "SushiDSP/log/log_sink.hpp"
#include "SushiDSP/log/log_formatter.hpp"

namespace SushiDSP
{
    /// @brief Formats log calls and hands each line at or above the minimum level to one sink.
    /// Never call it from the audio thread; see modules/foundation/log/README.md.
    class Logger final
    {
        public:
            /// @brief Starts with a sink and a minimum level.
            /// @param sink Must not be null.
            Logger(std::shared_ptr<LogSink> sink, LogLevel minimum_level) noexcept;

            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;
            Logger(Logger&&) = delete;
            Logger& operator=(Logger&&) = delete;
            ~Logger() = default;

            /// @brief Replaces the sink; lines already handed to the old one stay with it.
            /// @param sink Must not be null.
            /// @return The sink that was in place.
            std::shared_ptr<LogSink> set_sink(std::shared_ptr<LogSink> sink) noexcept;

            /// @brief Sets the level below which lines are discarded at run time.
            void set_minimum_level(LogLevel level) noexcept;

            /// @brief Returns the level below which lines are discarded at run time.
            [[nodiscard]] LogLevel minimum_level() const noexcept;

            /// @brief Reports whether a line of this level would reach the sink.
            [[nodiscard]] bool is_enabled(LogLevel level) const noexcept;

            /// @brief Formats a message on the stack and writes it when its level is enabled.
            /// @param format Positional `{}` markers only; see log_formatter.hpp.
            template <typename... Values>
            void log(LogLevel level, const LogCategory& category, std::string_view format,
                     const Values&... values) noexcept
            {
                if (!is_enabled(level))
                {
                    return;
                }
                char message[K_MAX_LOG_MESSAGE_LENGTH];
                const std::size_t length =
                    format_log_message(message, sizeof(message), format, values...);
                write(LogRecord{level, category, std::string_view(message, length)});
            }

            /// @brief Hands a formatted line to the sink whatever its level, and flushes after a Fatal.
            void write(const LogRecord& record) noexcept;

            /// @brief Returns once the sink has put every earlier line at its destination.
            void flush() noexcept;

        private:
            /// @brief Returns the sink in place, kept alive for the caller's use.
            std::shared_ptr<LogSink> current_sink() const noexcept;

            /// @brief Guards sink_; held only while the pointer is copied or swapped.
            mutable std::mutex sink_mutex_;

            /// @brief Holds the sink every line goes to.
            std::shared_ptr<LogSink> sink_;

            /// @brief Holds the runtime minimum level.
            std::atomic<LogLevel> minimum_level_;
    };
} // namespace SushiDSP
