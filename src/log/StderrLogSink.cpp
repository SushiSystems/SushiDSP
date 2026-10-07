/**************************************************************************/
/* StderrLogSink.cpp                                                      */
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
 * @file StderrLogSink.cpp
 * @brief Implements the synchronous stderr sink.
 * @author Mustafa Garip
 */

#include "SushiDSP/log/StderrLogSink.hpp"

#include <cstdio>

#include "SushiDSP/log/LogFormatter.hpp"

namespace SushiDSP
{
    namespace
    {
        /// @brief Sizes the room a line needs beyond its message: brackets, names and the newline.
        constexpr std::size_t kLinePrefixRoom = 96;
    }

    void StderrLogSink::write(const LogRecord& record) noexcept
    {
        char              line[kMaxLogMessageLength + kLinePrefixRoom];
        const std::size_t length =
            format_log_message(line, sizeof(line) - 1, "[{}] [{}] {}", log_level_name(record.level),
                               record.category.name(), record.message);
        line[length] = '\n';
        std::fwrite(line, 1, length + 1, stderr);
    }

    void StderrLogSink::flush() noexcept
    {
        std::fflush(stderr);
    }
}
