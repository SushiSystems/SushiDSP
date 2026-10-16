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
 * @brief Defines the SD_LOG macros and the compile-time ceiling that cuts them.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/log/default_logger.hpp>

/// @brief Numbers LogLevel::Trace for the preprocessor.
#define SD_LOG_LEVEL_TRACE 0
/// @brief Numbers LogLevel::Debug for the preprocessor.
#define SD_LOG_LEVEL_DEBUG 1
/// @brief Numbers LogLevel::Info for the preprocessor.
#define SD_LOG_LEVEL_INFO 2
/// @brief Numbers LogLevel::Warn for the preprocessor.
#define SD_LOG_LEVEL_WARN 3
/// @brief Numbers LogLevel::Error for the preprocessor.
#define SD_LOG_LEVEL_ERROR 4
/// @brief Numbers LogLevel::Fatal for the preprocessor.
#define SD_LOG_LEVEL_FATAL 5
/// @brief Numbers LogLevel::Off for the preprocessor.
#define SD_LOG_LEVEL_OFF 6

static_assert(static_cast<int>(SushiDSP::LogLevel::Trace) == SD_LOG_LEVEL_TRACE &&
                  static_cast<int>(SushiDSP::LogLevel::Off) == SD_LOG_LEVEL_OFF,
              "the SD_LOG_LEVEL numbers must match LogLevel");

#if !defined(SD_ENABLE_LOGGING)
/// @brief Turns every call below ERROR off in a translation unit when defined as 0.
#define SD_ENABLE_LOGGING 1
#endif

#if !SD_ENABLE_LOGGING
/// @brief Cuts every call below ERROR; ERROR and FATAL are never cut.
#define SD_LOG_CEILING SD_LOG_LEVEL_OFF
#elif defined(NDEBUG)
/// @brief Cuts TRACE out of an optimised translation unit.
#define SD_LOG_CEILING SD_LOG_LEVEL_DEBUG
#else
/// @brief Keeps every call in a debug translation unit.
#define SD_LOG_CEILING SD_LOG_LEVEL_TRACE
#endif

namespace SushiDSP
{
    /// @brief Returns this translation unit's compile-time ceiling, SD_LOG_CEILING as a level.
    /// Internal linkage keeps each unit's own answer, since NDEBUG may differ between them.
    static constexpr LogLevel log_ceiling() noexcept
    {
        return static_cast<LogLevel>(SD_LOG_CEILING);
    }
} // namespace SushiDSP

/// @brief Logs through the logger of the process, evaluating the values only when the level passes.
#define SD_LOG_WRITE(level, category, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
        ::SushiDSP::Logger& sd_log_logger = ::SushiDSP::default_logger();                          \
        if (sd_log_logger.is_enabled(level, category))                                             \
        {                                                                                          \
            sd_log_logger.log(level, category, ::SushiDSP::LogSite{__FILE__, __LINE__},            \
                              __VA_ARGS__);                                                        \
        }                                                                                          \
    } while (false)

#if SD_LOG_CEILING <= SD_LOG_LEVEL_TRACE
/// @brief Logs step-by-step flow, for a developer chasing one bug.
#define SD_LOG_TRACE(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Trace, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_TRACE(category, ...) ((void)0)
#endif

#if SD_LOG_CEILING <= SD_LOG_LEVEL_DEBUG
/// @brief Logs internal state at a decision point.
#define SD_LOG_DEBUG(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Debug, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_DEBUG(category, ...) ((void)0)
#endif

#if SD_LOG_CEILING <= SD_LOG_LEVEL_INFO
/// @brief Logs a lifecycle event an operator cares about.
#define SD_LOG_INFO(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Info, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_INFO(category, ...) ((void)0)
#endif

#if SD_LOG_CEILING <= SD_LOG_LEVEL_WARN
/// @brief Logs a recovery or a fallback, with what the program does next.
#define SD_LOG_WARN(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Warn, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_WARN(category, ...) ((void)0)
#endif

/// @brief Logs a failed operation whose caller sees a failure; never compiled out.
#define SD_LOG_ERROR(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Error, category, __VA_ARGS__)

/// @brief Logs why the process cannot continue, after the queue, and flushes; never compiled out.
#define SD_LOG_FATAL(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Fatal, category, __VA_ARGS__)