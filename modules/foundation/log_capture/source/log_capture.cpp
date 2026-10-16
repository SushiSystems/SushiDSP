/**************************************************************************/
/* log_capture.cpp                                                        */
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
 * @file log_capture.cpp
 * @brief Defines the scope that collects every SushiLog record.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <SushiLog/log/log_calls.hpp>
#include <SushiLog/log/log_record.hpp>
#include <SushiLog/log/log_sink.hpp>

namespace SushiDSP
{
    LogCapture::LogCapture()
    {
        SushiLog::set_log_synchronous(true);
        SushiLog::set_log_sink(SushiLog::LogSink(
            [this](const SushiLog::LogRecord& record)
            {
                const std::lock_guard<std::mutex> lock(mutex_);
                lines_.push_back(CapturedLogLine{record.level, std::string(record.module),
                                                 std::string(record.message),
                                                 std::string(record.text)});
            }));
    }

    LogCapture::~LogCapture()
    {
        SushiLog::set_log_sink(SushiLog::LogSink());
    }

    std::vector<CapturedLogLine> LogCapture::lines() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }
} // namespace SushiDSP