/**************************************************************************/
/* log.hpp                                                                */
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
 * @file log.hpp
 * @brief Declares the SD_LOG macros, SushiDSP's front over SushiLog.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_front.hpp>

#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

#if !defined(SD_ENABLE_LOGGING)
/// @brief Turns every call below ERROR off in a translation unit when defined as 0.
#define SD_ENABLE_LOGGING 1
#endif

namespace SushiDSP
{
    /// @brief Holds this translation unit's ceiling: ERROR when logging is off, DEBUG under
    /// NDEBUG, TRACE otherwise.
    [[maybe_unused]] static constexpr LogLevel K_SUSHIDSP_LOG_CEILING =
        SushiLog::log_level_ceiling(SD_ENABLE_LOGGING != 0);
} // namespace SushiDSP

/// @brief Logs at a level in a category through this translation unit's ceiling.
#define SD_LOG_AT(level, category, ...)                                                            \
    SL_LOG_FRONT_AT(::SushiDSP::K_SUSHIDSP_LOG_CEILING, level, category, __VA_ARGS__)

/// @brief Logs step-by-step flow, for a developer chasing one bug.
#define SD_LOG_TRACE(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Trace, category, __VA_ARGS__)

/// @brief Logs internal state at a decision point.
#define SD_LOG_DEBUG(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Debug, category, __VA_ARGS__)

/// @brief Logs a lifecycle event an operator cares about.
#define SD_LOG_INFO(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Info, category, __VA_ARGS__)

/// @brief Logs a recovery or a fallback, with what the program does next.
#define SD_LOG_WARN(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Warn, category, __VA_ARGS__)

/// @brief Logs a failed operation whose caller sees a failure status.
#define SD_LOG_ERROR(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Error, category, __VA_ARGS__)

/// @brief Logs why the process cannot continue, just before it ends.
#define SD_LOG_FATAL(category, ...) SD_LOG_AT(::SushiLog::LogLevel::Fatal, category, __VA_ARGS__)