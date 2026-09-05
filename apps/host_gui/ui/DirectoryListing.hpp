/**************************************************************************/
/* DirectoryListing.hpp                                                   */
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

#include <string>
#include <vector>

namespace ui
{
    /// @brief One entry in a browsed directory.
    struct DirectoryEntry
    {
        std::string name;          ///< The entry's own name, not a full path.
        bool        is_directory;
    };

    /// @brief Lists a directory for the browser: subdirectories first, then the files whose
    /// extension matches, each group sorted by name, case-insensitively.
    /// @param directory The directory to read.
    /// @param extension Lowercase extension to keep, e.g. ".wav". An empty string keeps every
    /// file.
    /// @return The entries, or an empty vector when directory does not exist, is not a
    /// directory, or cannot be read. Reading a directory can fail for reasons the caller cannot
    /// prevent, so this reports emptiness rather than throwing.
    std::vector<DirectoryEntry> list_directory(const std::string& directory,
                                               const std::string& extension);
}
