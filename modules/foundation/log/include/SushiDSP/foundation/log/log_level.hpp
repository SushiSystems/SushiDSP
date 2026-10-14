/**************************************************************************/
/* log_level.hpp                                                          */
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
 * @file log_level.hpp
 * @brief Declares the six severities a log line carries and their printed names.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

namespace SushiDSP
{
    /// @brief Orders the severities of a log line, least severe first.
    enum class LogLevel : std::uint8_t
    {
        Trace = 0, /**< Step-by-step flow for a developer chasing one defect. */
        Debug = 1, /**< Internal state at a decision point. */
        Info = 2,  /**< A lifecycle event an operator cares about. */
        Warn = 3,  /**< The program recovered or fell back. */
        Error = 4, /**< An operation failed and its caller sees a failure. */
        Fatal = 5, /**< The process cannot continue. */
    };

    /// @brief Returns the upper-case name a sink prints for a level.
    /// @return A string with static storage, never null.
    constexpr const char* log_level_name(LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        }
        return "UNKNOWN";
    }
} // namespace SushiDSP
