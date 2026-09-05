/**************************************************************************/
/* FileBrowser.hpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <optional>
#include <string>

namespace ui
{
    /// @brief Draws a directory listing the user can walk, and reports the file they picked.
    /// @param id The ImGui id, unique within the window.
    /// @param directory In/out: the directory on show. The browser rewrites it when the user
    /// opens a subdirectory or goes up, so the caller can remember where they were.
    /// @param extension The extension to offer, e.g. ".wav".
    /// @return The picked file's full path, or std::nullopt on every frame the user picks
    /// nothing.
    std::optional<std::string> FileBrowser(const char* id, std::string& directory,
                                           const std::string& extension);
}
