/**************************************************************************/
/* startup_rig.cpp                                                        */
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
 * @file startup_rig.cpp
 * @brief Implements the reader of the rig preset a plugin starts on when it has no state.
 * @author Mustafa Garip
 */

#include "startup_rig.hpp"

#include <SushiDSP/foundation/log/log.hpp>

#include "plugin_log_category.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace SushiDSP::Plugin
{
    std::optional<std::string> read_startup_rig()
    {
        const char* path = std::getenv(K_STARTUP_RIG_VARIABLE);
        if (path == nullptr || path[0] == '\0')
        {
            return std::nullopt;
        }
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            SD_LOG_WARN(K_PLUGIN_LOG_CATEGORY,
                        "{} names '{}', which could not be opened; the plugin starts on the empty "
                        "rig",
                        K_STARTUP_RIG_VARIABLE, path);
            return std::nullopt;
        }
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
} // namespace SushiDSP::Plugin
