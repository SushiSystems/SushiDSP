/**************************************************************************/
/* capturing_log_sink.hpp                                                 */
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
 * @file capturing_log_sink.hpp
 * @brief Declares the sink that keeps every line in memory for a test to read.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "SushiDSP/log/log_sink.hpp"

namespace SushiDSP
{
    /// @brief Holds an owned copy of one line a CapturingLogSink received.
    struct CapturedLogLine
    {
            LogLevel level;       /**< The severity of the line. */
            std::string category; /**< The name of the category the line was logged under. */
            std::string message;  /**< The formatted text. */
    };

    /// @brief Keeps every line it receives, in arrival order, before write() returns.
    class CapturingLogSink final : public LogSink
    {
        public:
            /// @brief Appends a copy of the line.
            void write(const LogRecord& record) noexcept override;

            /// @brief Counts the call; the lines are already stored.
            void flush() noexcept override;

            /// @brief Returns a copy of the lines received so far.
            std::vector<CapturedLogLine> lines() const;

            /// @brief Returns how many times flush() was called.
            std::size_t flush_count() const;

        private:
            /// @brief Guards lines_ and flush_count_.
            mutable std::mutex mutex_;

            /// @brief Holds the lines in arrival order.
            std::vector<CapturedLogLine> lines_;

            /// @brief Counts the flush() calls.
            std::size_t flush_count_ = 0;
    };
} // namespace SushiDSP
