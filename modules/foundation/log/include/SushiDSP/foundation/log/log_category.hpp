/**************************************************************************/
/* log_category.hpp                                                       */
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
 * @file log_category.hpp
 * @brief Declares the name a module logs under.
 * @author Mustafa Garip
 */

#pragma once

#include <string_view>

namespace SushiDSP
{
    /// @brief Names the module a log line comes from.
    class LogCategory final
    {
        public:
            /// @brief Wraps a module name.
            /// @param name Must outlive every line logged under the category; a string literal does.
            constexpr explicit LogCategory(std::string_view name) noexcept : name_(name) {}

            /// @brief Returns the module name.
            constexpr std::string_view name() const noexcept
            {
                return name_;
            }

        private:
            /// @brief Holds the module name; the category does not own its characters.
            std::string_view name_;
    };

    /// @brief Makes the category a module declares once, in its `<module>_log_category.hpp`.
    /// @param name Must outlive every line logged under the category; a string literal does.
    constexpr LogCategory make_log_category(std::string_view name) noexcept
    {
        return LogCategory(name);
    }
} // namespace SushiDSP
