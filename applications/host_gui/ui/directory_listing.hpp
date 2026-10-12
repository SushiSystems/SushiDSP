/**************************************************************************/
/* directory_listing.hpp                                                  */
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
 * @file directory_listing.hpp
 * @brief Declares the directory listing the file browser reads.
 * @author Mustafa Garip
 */

#pragma once

#include <string>
#include <vector>

namespace ui
{
    /// @brief One entry in a browsed directory.
    struct DirectoryEntry
    {
            std::string name; ///< The entry's own name, not a full path.
            bool is_directory;
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

    /// @brief Lists a directory for the browser, keeping the files of several extensions.
    /// @param directory The directory to read.
    /// @param extensions Lowercase extensions to keep, e.g. { ".wav", ".mp3" }. An empty list
    /// keeps every file.
    /// @return The entries, ordered as the single-extension listing orders them.
    std::vector<DirectoryEntry> list_directory(const std::string& directory,
                                               const std::vector<std::string>& extensions);
} // namespace ui
