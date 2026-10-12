/**************************************************************************/
/* log_formatter.cpp                                                      */
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
 * @file log_formatter.cpp
 * @brief Implements the positional `{}` formatter.
 * @author Mustafa Garip
 */

#include "SushiDSP/log/log_formatter.hpp"

#include <algorithm>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        /// @brief Holds the marker an argument replaces.
        constexpr std::string_view K_MARKER = "{}";
    } // namespace

    std::size_t format_log_arguments(char* buffer, std::size_t capacity, std::string_view format,
                                     const LogArgument* arguments,
                                     std::size_t argument_count) noexcept
    {
        std::size_t written = 0;
        std::size_t next_argument = 0;

        while (!format.empty() && written < capacity)
        {
            const std::size_t marker = format.find(K_MARKER);
            const bool replaces =
                marker != std::string_view::npos && next_argument < argument_count;
            const std::size_t literal = replaces ? marker : format.size();
            const std::size_t copied = std::min(literal, capacity - written);

            std::memcpy(buffer + written, format.data(), copied);
            written += copied;
            if (!replaces)
            {
                break;
            }

            written += arguments[next_argument].write_to(buffer + written, capacity - written);
            ++next_argument;
            format.remove_prefix(marker + K_MARKER.size());
        }

        return written;
    }
} // namespace SushiDSP
