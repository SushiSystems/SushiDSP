/**************************************************************************/
/* stderr_log_sink.cpp                                                    */
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
 * @file stderr_log_sink.cpp
 * @brief Implements the synchronous stderr sink.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/stderr_log_sink.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace SushiDSP
{
    void StderrLogSink::write(const LogRecord& record) noexcept
    {
        char line[K_MAX_LOG_LINE_LENGTH + 1];
        const std::string_view text = record.text.empty() ? record.message : record.text;
        const std::size_t length = std::min(text.size(), K_MAX_LOG_LINE_LENGTH);
        std::memcpy(line, text.data(), length);
        line[length] = '\n';
        std::fwrite(line, 1, length + 1, stderr);
    }

    void StderrLogSink::flush() noexcept
    {
        std::fflush(stderr);
    }
} // namespace SushiDSP
