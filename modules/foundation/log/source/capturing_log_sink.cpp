/**************************************************************************/
/* capturing_log_sink.cpp                                                 */
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
 * @file capturing_log_sink.cpp
 * @brief Implements the in-memory sink tests read their log lines from.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>

namespace SushiDSP
{
    void CapturingLogSink::write(const LogRecord& record)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        lines_.push_back(CapturedLogLine{record.level, std::string(record.module),
                                         std::string(record.message), std::string(record.text)});
    }

    void CapturingLogSink::flush() noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        ++flush_count_;
    }

    std::vector<CapturedLogLine> CapturingLogSink::lines() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }

    std::size_t CapturingLogSink::flush_count() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return flush_count_;
    }
} // namespace SushiDSP
