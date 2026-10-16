/**************************************************************************/
/* log_level_setting.cpp                                                  */
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
 * @file log_level_setting.cpp
 * @brief Implements the reading of a log level setting.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/log/log_level_setting.hpp>

#include <SushiDSP/foundation/log/log_category.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Returns a view with the spaces and tabs at both ends removed.
        std::string_view trimmed(std::string_view text) noexcept
        {
            const std::size_t first = text.find_first_not_of(" \t");
            if (first == std::string_view::npos)
                return {};
            const std::size_t last = text.find_last_not_of(" \t");
            return text.substr(first, last - first + 1);
        }

        /// @brief Reports whether two names match, ignoring the case of ASCII letters.
        bool equals_ignoring_case(std::string_view left, std::string_view right) noexcept
        {
            if (left.size() != right.size())
                return false;
            for (std::size_t index = 0; index < left.size(); ++index)
            {
                const char a = left[index];
                const char lower = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + 32) : a;
                if (lower != right[index])
                    return false;
            }
            return true;
        }

        /// @brief Holds each level beside the name a setting writes it with.
        struct NamedLevel
        {
                std::string_view name; /**< The lower-case name. */
                LogLevel level;        /**< The level it names. */
        };

        /// @brief Lists the names parse_log_level() reads.
        constexpr NamedLevel K_LEVEL_NAMES[] = {
            {"trace", LogLevel::Trace}, {"debug", LogLevel::Debug}, {"info", LogLevel::Info},
            {"warn", LogLevel::Warn},   {"error", LogLevel::Error}, {"fatal", LogLevel::Fatal},
            {"off", LogLevel::Off},
        };
    } // namespace

    std::optional<LogLevel> parse_log_level(std::string_view name) noexcept
    {
        for (const NamedLevel& entry : K_LEVEL_NAMES)
        {
            if (equals_ignoring_case(name, entry.name))
                return entry.level;
        }
        return std::nullopt;
    }

    std::optional<LogLevelSetting> parse_log_level_setting(std::string_view text) noexcept
    {
        LogLevelSetting setting;
        bool first = true;
        std::size_t start = 0;
        for (;;)
        {
            const std::size_t comma = text.find(',', start);
            const std::string_view item = trimmed(
                text.substr(start, comma == std::string_view::npos ? std::string_view::npos
                                                                   : comma - start));
            const std::size_t equals = item.find('=');
            if (first)
            {
                const std::optional<LogLevel> every = parse_log_level(item);
                if (!every)
                    return std::nullopt;
                setting.every = *every;
                first = false;
            }
            else
            {
                if (equals == std::string_view::npos || setting.pair_count == setting.pairs.size())
                    return std::nullopt;
                const std::string_view category = trimmed(item.substr(0, equals));
                const std::optional<LogLevel> level = parse_log_level(trimmed(item.substr(equals + 1)));
                if (category.empty() || !level)
                    return std::nullopt;
                setting.pairs[setting.pair_count++] = {log_category_hash(category), *level};
            }
            if (comma == std::string_view::npos)
                return setting;
            start = comma + 1;
        }
    }
} // namespace SushiDSP
