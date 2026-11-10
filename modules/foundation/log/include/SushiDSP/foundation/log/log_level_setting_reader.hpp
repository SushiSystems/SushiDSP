/**************************************************************************/
/* log_level_setting_reader.hpp                                           */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file log_level_setting_reader.hpp
 * @brief Declares the reading of a project's level variable through an EnvironmentReader and its hand-over to SushiLog.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>

#include <SushiLog/log/level_setting.hpp>
#include <SushiLog/log/log_calls.hpp>
#include <SushiLog/log/log_category.hpp>
#include <SushiLog/log/log_project.hpp>

#include <SushiDSP/foundation/log/environment_reader.hpp>

namespace SushiDSP
{
    /// @brief Reads the level variable of @p project through @p environment.
    /// @return Nothing when the variable is unset or SushiLog cannot read its value.
    [[nodiscard]] inline std::optional<SushiLog::LevelSetting> read_log_level_setting(
        SushiLog::LogProject project, const EnvironmentReader& environment)
    {
        const std::optional<std::string> value = environment.read(project.level_variable());
        if (!value.has_value())
        {
            return std::nullopt;
        }
        return SushiLog::parse_level_setting(*value);
    }

    /// @brief Hands the level variable of @p project, read through @p environment, to SushiLog:
    /// the project's level, then each module=level pair as its category's level. Does nothing
    /// when read_log_level_setting() finds nothing.
    inline void apply_log_level_setting(SushiLog::LogProject project,
                                        const EnvironmentReader& environment)
    {
        const std::optional<SushiLog::LevelSetting> setting =
            read_log_level_setting(project, environment);
        if (!setting.has_value())
        {
            return;
        }
        SushiLog::set_log_level(project, setting->level);
        for (const SushiLog::ModuleLevel& module : setting->modules)
        {
            SushiLog::set_log_level(SushiLog::make_log_category(project, module.module),
                                    module.level);
        }
    }
} // namespace SushiDSP
