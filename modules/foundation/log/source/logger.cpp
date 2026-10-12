/**************************************************************************/
/* logger.cpp                                                             */
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
 * @file logger.cpp
 * @brief Implements the logger's sink ownership and its runtime level filter.
 * @author Mustafa Garip
 */

#include "SushiDSP/log/logger.hpp"

#include <utility>

namespace SushiDSP
{
    Logger::Logger(std::shared_ptr<LogSink> sink, LogLevel minimum_level) noexcept
        : sink_(std::move(sink)), minimum_level_(minimum_level)
    {
    }

    std::shared_ptr<LogSink> Logger::set_sink(std::shared_ptr<LogSink> sink) noexcept
    {
        const std::lock_guard<std::mutex> lock(sink_mutex_);
        sink_.swap(sink);
        return sink;
    }

    void Logger::set_minimum_level(LogLevel level) noexcept
    {
        minimum_level_.store(level, std::memory_order_relaxed);
    }

    LogLevel Logger::minimum_level() const noexcept
    {
        return minimum_level_.load(std::memory_order_relaxed);
    }

    bool Logger::is_enabled(LogLevel level) const noexcept
    {
        return level >= minimum_level();
    }

    void Logger::write(const LogRecord& record) noexcept
    {
        const std::shared_ptr<LogSink> sink = current_sink();
        sink->write(record);
        if (record.level == LogLevel::Fatal)
        {
            sink->flush();
        }
    }

    void Logger::flush() noexcept
    {
        current_sink()->flush();
    }

    std::shared_ptr<LogSink> Logger::current_sink() const noexcept
    {
        const std::lock_guard<std::mutex> lock(sink_mutex_);
        return sink_;
    }
} // namespace SushiDSP
