/**************************************************************************/
/* log_line.hpp                                                           */
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
 * @file log_line.hpp
 * @brief Declares the one shape a record line has, and the function that writes it.
 * @author Mustafa Garip
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <SushiDSP/foundation/log/log_level.hpp>

namespace SushiDSP
{
    /// @brief Holds the fields a record line is written from.
    struct LogLineFields
    {
            std::chrono::system_clock::time_point time; /**< When the call was made. */
            LogLevel level;                             /**< The severity of the record. */
            std::string_view project;                   /**< The project name. */
            std::string_view module;                    /**< The module name. */
            std::uint32_t thread;                       /**< current_log_thread_number() of the caller. */
            std::string_view file;                      /**< The call's file name, no folders. */
            std::uint32_t line;                         /**< The call's line. */
            std::string_view message;                   /**< The formatted message. */
    };

    /// @brief Writes `[hh:mm:ss.mmm] [LEVEL] [project.module] [tN] [file:line] message`, local time.
    /// The fields ahead of the message are cut at K_MAX_LOG_PREFIX_LENGTH; the message is whole.
    /// @param capacity Must be at least K_MAX_LOG_PREFIX_LENGTH plus the message length.
    /// @return The characters written; the message is the last message.size() of them.
    std::size_t format_log_line(char* buffer, std::size_t capacity,
                                const LogLineFields& fields) noexcept;
} // namespace SushiDSP
