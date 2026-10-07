/**************************************************************************/
/* LogFormatter.hpp                                                       */
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
 * @file LogFormatter.hpp
 * @brief Declares the positional `{}` formatter log calls go through.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <string_view>

#include "SushiDSP/log/LogArgument.hpp"

namespace SushiDSP
{
    /**
     * @brief Replaces each `{}` of a format with the next argument, in order.
     * A marker with no argument left prints as `{}`; an argument with no marker is not printed.
     * @param buffer Receives the text, cut at capacity, with no terminator.
     * @param arguments May be null when argument_count is zero.
     * @return The number of characters written, at most capacity.
     */
    std::size_t format_log_arguments(char* buffer, std::size_t capacity, std::string_view format,
                                     const LogArgument* arguments,
                                     std::size_t argument_count) noexcept;

    /// @brief Copies a format that takes no values, so each `{}` in it prints as `{}`.
    /// @param buffer Receives the text, cut at capacity, with no terminator.
    /// @return The number of characters written, at most capacity.
    inline std::size_t format_log_message(char* buffer, std::size_t capacity,
                                          std::string_view format) noexcept
    {
        return format_log_arguments(buffer, capacity, format, nullptr, 0);
    }

    /// @brief Formats a message from values of the kinds LogArgument captures.
    /// @param buffer Receives the text, cut at capacity, with no terminator.
    /// @return The number of characters written, at most capacity.
    template<typename First, typename... Rest>
    std::size_t format_log_message(char* buffer, std::size_t capacity, std::string_view format,
                                   const First& first, const Rest&... rest) noexcept
    {
        const LogArgument arguments[] = { LogArgument(first), LogArgument(rest)... };
        return format_log_arguments(buffer, capacity, format, arguments, 1 + sizeof...(Rest));
    }
}
