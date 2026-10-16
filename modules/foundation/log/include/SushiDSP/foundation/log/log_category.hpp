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
 * @brief Declares the name a module logs under and the hash its level is found by.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <string_view>

#include <SushiDSP/foundation/log/log_project.hpp>

namespace SushiDSP
{
    /// @brief Returns the FNV-1a hash of a category name, never zero, ignoring letter case.
    constexpr std::uint64_t log_category_hash(std::string_view name) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const char character : name)
        {
            const char lower =
                (character >= 'A' && character <= 'Z') ? static_cast<char>(character + 32) : character;
            hash ^= static_cast<unsigned char>(lower);
            hash *= 1099511628211ull;
        }
        return hash == 0 ? 1 : hash;
    }

    /// @brief Names the project and the module a log record comes from.
    class LogCategory final
    {
        public:
            /// @brief Holds the project and the module name, and hashes the module name.
            /// @param module Must outlive every record logged under the category; a literal does.
            constexpr LogCategory(LogProject project, std::string_view module) noexcept
                : project_(project), module_(module), hash_(log_category_hash(module))
            {
            }

            /// @brief Returns the project the category belongs to.
            constexpr LogProject project() const noexcept
            {
                return project_;
            }

            /// @brief Returns the module name.
            constexpr std::string_view module() const noexcept
            {
                return module_;
            }

            /// @brief Returns the hash the logger finds the category's level by.
            constexpr std::uint64_t hash() const noexcept
            {
                return hash_;
            }

        private:
            /// @brief Holds the project the category belongs to.
            LogProject project_;

            /// @brief Holds the module name; the category does not own its characters.
            std::string_view module_;

            /// @brief Holds log_category_hash() of the module name.
            std::uint64_t hash_;
    };

    /// @brief Makes the category a module declares once, in its `<module>_log_category.hpp`.
    /// @param module Must outlive every record logged under the category; a literal does.
    constexpr LogCategory make_log_category(LogProject project, std::string_view module) noexcept
    {
        return LogCategory(project, module);
    }
} // namespace SushiDSP