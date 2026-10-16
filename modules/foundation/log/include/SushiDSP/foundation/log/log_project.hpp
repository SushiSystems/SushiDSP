/**************************************************************************/
/* log_project.hpp                                                        */
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
 * @file log_project.hpp
 * @brief Declares the project a log category belongs to.
 * @author Mustafa Garip
 */

#pragma once

#include <string_view>

namespace SushiDSP
{
    /// @brief Names a project and the environment variable that sets its levels.
    /// It holds views of string literals, so it lives for the whole process.
    class LogProject final
    {
        public:
            /// @brief Holds the project's name and its level variable.
            constexpr LogProject(std::string_view name, std::string_view level_variable) noexcept
                : name_(name), level_variable_(level_variable)
            {
            }

            /// @brief Returns the project's name, the first part of a record's category field.
            constexpr std::string_view name() const noexcept
            {
                return name_;
            }

            /// @brief Returns the environment variable read at the project's first log call.
            constexpr std::string_view level_variable() const noexcept
            {
                return level_variable_;
            }

        private:
            /// @brief Holds the project's name.
            std::string_view name_;

            /// @brief Holds the name of the project's level variable.
            std::string_view level_variable_;
    };
} // namespace SushiDSP
