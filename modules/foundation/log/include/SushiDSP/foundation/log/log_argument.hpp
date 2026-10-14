/**************************************************************************/
/* log_argument.hpp                                                       */
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
 * @file log_argument.hpp
 * @brief Declares the type-erased value a `{}` marker prints.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace SushiDSP
{
    /// @brief Holds one value of a log call: a bool, a character, an integer, a real or text.
    class LogArgument final
    {
        public:
            /// @brief Captures a bool, a char, an integer or a floating-point value.
            template <typename Number, std::enable_if_t<std::is_arithmetic_v<Number>, int> = 0>
            explicit LogArgument(Number value) noexcept
            {
                if constexpr (std::is_same_v<Number, bool>)
                {
                    kind_ = Kind::Boolean;
                    value_.unsigned_value = value ? 1u : 0u;
                }
                else if constexpr (std::is_same_v<Number, char>)
                {
                    kind_ = Kind::Character;
                    value_.signed_value = value;
                }
                else if constexpr (std::is_floating_point_v<Number>)
                {
                    kind_ = Kind::Real;
                    value_.real_value = static_cast<double>(value);
                }
                else if constexpr (std::is_signed_v<Number>)
                {
                    kind_ = Kind::Signed;
                    value_.signed_value = value;
                }
                else
                {
                    kind_ = Kind::Unsigned;
                    value_.unsigned_value = value;
                }
            }

            /// @brief Captures a C string; a null pointer prints as `(null)`.
            /// @param text Must stay valid until the argument is written.
            explicit LogArgument(const char* text) noexcept;

            /// @brief Captures a string view.
            /// @param text Its characters must stay valid until the argument is written.
            explicit LogArgument(std::string_view text) noexcept;

            /// @brief Captures a string.
            /// @param text Must stay valid until the argument is written.
            explicit LogArgument(const std::string& text) noexcept;

            /// @brief Writes the value as text, cut at the end of the buffer, with no terminator.
            /// @return The number of characters written, at most capacity.
            std::size_t write_to(char* buffer, std::size_t capacity) const noexcept;

        private:
            /// @brief Names which member of Value is in use.
            enum class Kind : std::uint8_t
            {
                Boolean,   /**< unsigned_value, printed as true or false. */
                Character, /**< signed_value, printed as one character. */
                Signed,    /**< signed_value, printed in decimal. */
                Unsigned,  /**< unsigned_value, printed in decimal. */
                Real,      /**< real_value, printed with six significant digits. */
                Text,      /**< text_data and text_size_, printed as they are. */
            };

            /// @brief Holds the captured value in the member Kind names.
            union Value
            {
                    long long signed_value;            /**< A signed integer or a character code. */
                    unsigned long long unsigned_value; /**< An unsigned integer or a bool. */
                    double real_value;                 /**< A floating-point value. */
                    const char* text_data;             /**< The first character of a text value. */
            };

            /// @brief Names the member of value_ that is in use.
            Kind kind_ = Kind::Text;

            /// @brief Holds the captured value.
            Value value_ = {};

            /// @brief Counts the characters behind text_data when kind_ is Text.
            std::size_t text_size_ = 0;
    };
} // namespace SushiDSP
