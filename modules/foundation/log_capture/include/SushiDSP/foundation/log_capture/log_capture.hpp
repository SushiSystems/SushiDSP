/**************************************************************************/
/* log_capture.hpp                                                        */
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
 * @file log_capture.hpp
 * @brief Declares the scope that collects every SushiLog record, for tests.
 * @author Mustafa Garip
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <SushiLog/log/log_level.hpp>

namespace SushiDSP
{
    /// @brief Holds one record a LogCapture collected.
    struct CapturedLogLine
    {
            SushiLog::LogLevel level; /**< The severity of the record. */
            std::string category; /**< The module of the record's category. */
            std::string message;  /**< The formatted message. */
            std::string line;     /**< The whole record line. */
    };

    /// @brief Delivers every record synchronously into a list for its lifetime, then restores
    /// stderr. For tests only: one capture at a time, and a sink set before it is not restored.
    class LogCapture final
    {
        public:
            /// @brief Starts collecting.
            LogCapture();

            /// @brief Stops collecting and restores the stderr sink.
            ~LogCapture();

            LogCapture(const LogCapture&) = delete;
            LogCapture& operator=(const LogCapture&) = delete;
            LogCapture(LogCapture&&) = delete;
            LogCapture& operator=(LogCapture&&) = delete;

            /// @brief Returns a copy of every record collected so far, oldest first.
            [[nodiscard]] std::vector<CapturedLogLine> lines() const;

        private:
            /// @brief Guards lines_.
            mutable std::mutex mutex_;

            /// @brief Holds the collected records.
            std::vector<CapturedLogLine> lines_;
    };
} // namespace SushiDSP