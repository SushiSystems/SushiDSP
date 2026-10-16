/**
 * @file sushidsp_log_project.hpp
 * @brief Declares the SushiLog project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiLog/log/log_category.hpp>
#include <SushiLog/log/log_level.hpp>
#include <SushiLog/log/log_project.hpp>

namespace SushiDSP
{
    /// @brief Names the level of a SushiDSP record; SushiLog defines the levels.
    using LogLevel = SushiLog::LogLevel;

    /// @brief Names the module a SushiDSP record comes from.
    using LogCategory = SushiLog::LogCategory;

    /// @brief Makes a category from the project and a module name.
    using SushiLog::make_log_category;

    /// @brief Holds the SushiDSP project, whose levels SD_LOG_LEVEL sets.
    inline constexpr SushiLog::LogProject K_SUSHIDSP_LOG_PROJECT{"sushidsp", "SD_LOG_LEVEL"};
} // namespace SushiDSP