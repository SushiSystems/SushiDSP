/**************************************************************************/
/* log_level_setting.hpp                                                  */
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
 * @file log_level_setting.hpp
 * @brief Declares the parsed form of a log level setting such as `warn,room=debug`.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include <SushiDSP/foundation/log/log_level.hpp>
#include <SushiDSP/foundation/log/log_level_table.hpp>

namespace SushiDSP
{
    /// @brief Holds one `category=level` pair of a setting.
    struct LogCategoryLevel
    {
            std::uint64_t category_hash; /**< log_category_hash() of the category name. */
            LogLevel level;              /**< The level the category is set to. */
    };

    /// @brief Holds a level for every category and the pairs that override it.
    struct LogLevelSetting
    {
            LogLevel every = LogLevel::Warn;                          /**< The first item. */
            std::array<LogCategoryLevel, K_MAX_LOG_CATEGORIES> pairs{}; /**< The pairs, in order. */
            std::size_t pair_count = 0;                               /**< The pairs in use. */
    };

    /// @brief Reads a level name in any case: trace, debug, info, warn, error, fatal or off.
    [[nodiscard]] std::optional<LogLevel> parse_log_level(std::string_view name) noexcept;

    /// @brief Reads a level for every category, then any number of `category=level` pairs.
    /// Items are separated by commas; spaces around an item are ignored; case does not matter.
    /// @return Nothing when any item cannot be read or there are more pairs than the table holds.
    [[nodiscard]] std::optional<LogLevelSetting> parse_log_level_setting(
        std::string_view text) noexcept;
} // namespace SushiDSP
