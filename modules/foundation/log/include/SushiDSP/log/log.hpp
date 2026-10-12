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
 * @brief Defines the SD_LOG macros and the compile-time minimum level that cuts them.
 * @author Mustafa Garip
 */

#pragma once

#include "SushiDSP/log/default_logger.hpp"

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

static_assert(static_cast<int>(SushiDSP::LogLevel::Trace) == SD_LOG_LEVEL_TRACE &&
                  static_cast<int>(SushiDSP::LogLevel::Fatal) == SD_LOG_LEVEL_FATAL,
              "the SD_LOG_LEVEL numbers must match LogLevel");

#if !defined(SD_LOG_MIN_LEVEL)
#if defined(NDEBUG)
/// @brief Cuts every log call below this level out of the translation unit; INFO in release.
#define SD_LOG_MIN_LEVEL SD_LOG_LEVEL_INFO
#else
/// @brief Cuts every log call below this level out of the translation unit; TRACE in debug.
#define SD_LOG_MIN_LEVEL SD_LOG_LEVEL_TRACE
#endif
#endif

/// @brief Logs through the logger of the process, evaluating the values only when the level is on.
#define SD_LOG_WRITE(level, category, ...)                                                         \
    do                                                                                             \
    {                                                                                              \
        ::SushiDSP::Logger& sd_log_logger = ::SushiDSP::default_logger();                          \
        if (sd_log_logger.is_enabled(level))                                                       \
        {                                                                                          \
            sd_log_logger.log(level, category, __VA_ARGS__);                                       \
        }                                                                                          \
    } while (false)

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_TRACE
/// @brief Logs step-by-step flow: `SD_LOG_TRACE(category, "format {}", values...)`.
#define SD_LOG_TRACE(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Trace, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_TRACE(category, ...) ((void)0)
#endif

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_DEBUG
/// @brief Logs internal state at a decision point.
#define SD_LOG_DEBUG(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Debug, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_DEBUG(category, ...) ((void)0)
#endif

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_INFO
/// @brief Logs a lifecycle event an operator cares about.
#define SD_LOG_INFO(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Info, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_INFO(category, ...) ((void)0)
#endif

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_WARN
/// @brief Logs a recovery or a fallback, with what the program does next.
#define SD_LOG_WARN(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Warn, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_WARN(category, ...) ((void)0)
#endif

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_ERROR
/// @brief Logs a failed operation whose caller sees a failure.
#define SD_LOG_ERROR(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Error, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_ERROR(category, ...) ((void)0)
#endif

#if SD_LOG_MIN_LEVEL <= SD_LOG_LEVEL_FATAL
/// @brief Logs why the process cannot continue and flushes the sink.
#define SD_LOG_FATAL(category, ...) SD_LOG_WRITE(::SushiDSP::LogLevel::Fatal, category, __VA_ARGS__)
#else
/// @brief Compiles to nothing; the arguments are not evaluated.
#define SD_LOG_FATAL(category, ...) ((void)0)
#endif
