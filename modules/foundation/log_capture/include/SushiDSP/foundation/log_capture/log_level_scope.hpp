/**************************************************************************/
/* log_level_scope.hpp                                                    */
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
 * @file log_level_scope.hpp
 * @brief Declares the scope that sets one SushiLog category's level, for tests.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_calls.hpp>
#include <SushiLog/log/log_category.hpp>
#include <SushiLog/log/log_level.hpp>

namespace SushiDSP
{
    /// @brief Sets one category's level for its lifetime, then sets the level it found back.
    /// For tests only: the category stays set one by one after the scope ends.
    class LogLevelScope final
    {
        public:
            /// @brief Sets @p category to @p level.
            LogLevelScope(SushiLog::LogCategory category, SushiLog::LogLevel level) noexcept
                : category_(category), previous_(SushiLog::log_level(category))
            {
                SushiLog::set_log_level(category_, level);
            }

            /// @brief Sets the category back to the level it had before the scope.
            ~LogLevelScope()
            {
                SushiLog::set_log_level(category_, previous_);
            }

            LogLevelScope(const LogLevelScope&) = delete;
            LogLevelScope& operator=(const LogLevelScope&) = delete;
            LogLevelScope(LogLevelScope&&) = delete;
            LogLevelScope& operator=(LogLevelScope&&) = delete;

        private:
            /// @brief Holds the category this scope sets.
            SushiLog::LogCategory category_;

            /// @brief Holds the level the category had before the scope.
            SushiLog::LogLevel previous_;
    };
} // namespace SushiDSP
