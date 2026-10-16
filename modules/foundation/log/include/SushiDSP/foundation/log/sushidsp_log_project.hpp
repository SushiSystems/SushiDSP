/**************************************************************************/
/* sushidsp_log_project.hpp                                               */
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