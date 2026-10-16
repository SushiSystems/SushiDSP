/**************************************************************************/
/* log_line.cpp                                                           */
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
 * @file log_line.cpp
 * @brief Implements the record line: local time, level, category, thread, site and message.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/log_line.hpp>

#include <algorithm>
#include <cstring>
#include <ctime>

#include <SushiDSP/foundation/log/log_formatter.hpp>
#include <SushiDSP/foundation/log/log_record.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Writes a value as exactly two or three digits; returns the characters written.
        std::size_t write_digits(char* out, unsigned value, std::size_t digits) noexcept
        {
            for (std::size_t index = digits; index > 0; --index)
            {
                out[index - 1] = static_cast<char>('0' + value % 10);
                value /= 10;
            }
            return digits;
        }

        /// @brief Returns the local wall-clock time of a point.
        std::tm local_time_of(std::time_t seconds) noexcept
        {
            std::tm local{};
#if defined(_WIN32)
            localtime_s(&local, &seconds);
#else
            localtime_r(&seconds, &local);
#endif
            return local;
        }

        /// @brief Writes `hh:mm:ss.mmm` into a buffer of at least twelve characters.
        std::size_t write_clock(char* out, std::chrono::system_clock::time_point time) noexcept
        {
            const auto since_epoch = time.time_since_epoch();
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch).count() % 1000;
            const std::tm local = local_time_of(std::chrono::system_clock::to_time_t(time));
            std::size_t length = write_digits(out, static_cast<unsigned>(local.tm_hour), 2);
            out[length++] = ':';
            length += write_digits(out + length, static_cast<unsigned>(local.tm_min), 2);
            out[length++] = ':';
            length += write_digits(out + length, static_cast<unsigned>(local.tm_sec), 2);
            out[length++] = '.';
            length += write_digits(out + length, static_cast<unsigned>(milliseconds), 3);
            return length;
        }

        /// @brief Holds the characters `hh:mm:ss.mmm` takes.
        constexpr std::size_t K_CLOCK_LENGTH = 12;
    } // namespace

    std::size_t format_log_line(char* buffer, std::size_t capacity,
                                const LogLineFields& fields) noexcept
    {
        char clock[K_CLOCK_LENGTH + 1];
        const std::size_t clock_length = write_clock(clock, fields.time);
        const std::size_t prefix_capacity =
            std::min(K_MAX_LOG_PREFIX_LENGTH, capacity - std::min(capacity, fields.message.size()));
        const std::size_t prefix_length = format_log_message(
            buffer, prefix_capacity, "[{}] [{}] [{}.{}] [t{}] [{}:{}] ",
            std::string_view(clock, clock_length), log_level_name(fields.level), fields.project,
            fields.module, fields.thread, fields.file, fields.line);
        const std::size_t message_length = std::min(fields.message.size(), capacity - prefix_length);
        if (message_length != 0)
        {
            std::memcpy(buffer + prefix_length, fields.message.data(), message_length);
        }
        return prefix_length + message_length;
    }
} // namespace SushiDSP
