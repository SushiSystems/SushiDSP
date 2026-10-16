/**************************************************************************/
/* log_record.hpp                                                         */
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
 * @file log_record.hpp
 * @brief Declares the one record a logger hands to a sink and the lengths that bound it.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <SushiDSP/foundation/log/log_level.hpp>

namespace SushiDSP
{
    /// @brief Bounds a formatted message in characters; a longer one is cut at this length.
    inline constexpr std::size_t K_MAX_LOG_MESSAGE_LENGTH = 1024;

    /// @brief Bounds the fields ahead of the message: time, level, category, thread, file and line.
    inline constexpr std::size_t K_MAX_LOG_PREFIX_LENGTH = 192;

    /// @brief Bounds a whole record line in characters, its newline left out.
    inline constexpr std::size_t K_MAX_LOG_LINE_LENGTH =
        K_MAX_LOG_PREFIX_LENGTH + K_MAX_LOG_MESSAGE_LENGTH;

    /// @brief Bounds a project, module or file name a queued record keeps.
    inline constexpr std::size_t K_MAX_LOG_NAME_LENGTH = 64;

    /// @brief Holds one record on its way to a sink; every view is valid only during the call.
    struct LogRecord
    {
            LogLevel level;           /**< The severity of the record. */
            std::string_view project; /**< The project the record comes from. */
            std::string_view module;  /**< The module the record comes from. */
            std::string_view file;    /**< The call's file name, without its folders. */
            std::uint32_t line;       /**< The call's line. */
            std::uint32_t thread;     /**< The sending logger's number for the calling thread. */
            std::int64_t time_ns;     /**< System clock time of the call, ns since the Unix epoch. */
            std::string_view message; /**< The formatted message alone. */
            std::string_view text;    /**< The whole record line, ending in the message. */
    };
} // namespace SushiDSP