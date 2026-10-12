/**************************************************************************/
/* log_argument.cpp                                                       */
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
 * @file log_argument.cpp
 * @brief Implements how each kind of log value prints.
 * @author Mustafa Garip
 */

#include "SushiDSP/log/log_argument.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace SushiDSP
{
    namespace
    {
        /// @brief Holds the text a null C string prints as.
        constexpr std::string_view K_NULL_TEXT = "(null)";

        /// @brief Sizes the scratch a number prints into; a 64-bit integer needs 20 characters.
        constexpr std::size_t K_NUMBER_SCRATCH = 32;

        /// @brief Copies as much of a text as a buffer holds.
        /// @return The number of characters copied.
        std::size_t copy_bounded(char* buffer, std::size_t capacity, const char* text,
                                 std::size_t size) noexcept
        {
            const std::size_t copied = std::min(capacity, size);
            if (copied != 0)
            {
                std::memcpy(buffer, text, copied);
            }
            return copied;
        }

        /// @brief Writes an integer in decimal, cut at the end of the buffer.
        /// @return The number of characters written.
        template <typename Integer>
        std::size_t write_integer(char* buffer, std::size_t capacity, Integer value) noexcept
        {
            char scratch[K_NUMBER_SCRATCH];
            const std::to_chars_result result =
                std::to_chars(scratch, scratch + K_NUMBER_SCRATCH, value);
            return copy_bounded(buffer, capacity, scratch,
                                static_cast<std::size_t>(result.ptr - scratch));
        }

        /// @brief Writes a real with six significant digits, cut at the end of the buffer.
        /// @return The number of characters written.
        std::size_t write_real(char* buffer, std::size_t capacity, double value) noexcept
        {
            char scratch[K_NUMBER_SCRATCH];
            const int printed = std::snprintf(scratch, K_NUMBER_SCRATCH, "%g", value);
            const std::size_t size =
                printed < 0 ? 0u
                            : std::min(static_cast<std::size_t>(printed), K_NUMBER_SCRATCH - 1);
            return copy_bounded(buffer, capacity, scratch, size);
        }
    } // namespace

    LogArgument::LogArgument(const char* text) noexcept
        : LogArgument(text == nullptr ? K_NULL_TEXT : std::string_view(text))
    {
    }

    LogArgument::LogArgument(std::string_view text) noexcept : text_size_(text.size())
    {
        value_.text_data = text.data();
    }

    LogArgument::LogArgument(const std::string& text) noexcept : LogArgument(std::string_view(text))
    {
    }

    std::size_t LogArgument::write_to(char* buffer, std::size_t capacity) const noexcept
    {
        switch (kind_)
        {
        case Kind::Boolean:
        {
            const std::string_view text = value_.unsigned_value != 0 ? "true" : "false";
            return copy_bounded(buffer, capacity, text.data(), text.size());
        }
        case Kind::Character:
        {
            const char character = static_cast<char>(value_.signed_value);
            return copy_bounded(buffer, capacity, &character, 1);
        }
        case Kind::Signed:
            return write_integer(buffer, capacity, value_.signed_value);
        case Kind::Unsigned:
            return write_integer(buffer, capacity, value_.unsigned_value);
        case Kind::Real:
            return write_real(buffer, capacity, value_.real_value);
        case Kind::Text:
            return copy_bounded(buffer, capacity, value_.text_data, text_size_);
        }
        return 0;
    }
} // namespace SushiDSP
