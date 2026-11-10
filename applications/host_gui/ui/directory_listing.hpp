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
 * @file directory_listing.hpp
 * @brief Declares the directory listing the file browser reads.
 * @author Mustafa Garip
 */

#pragma once

#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace ui
{
    /// @brief One entry in a browsed directory.
    struct DirectoryEntry
    {
            std::string name; ///< The entry's own name, not a full path.
            bool is_directory;
    };

    /// @brief Where a listing reads a directory's entries from.
    class DirectorySource
    {
        public:
            virtual ~DirectorySource() = default;

            /// @brief Reads every entry of a directory, unfiltered and in no particular order.
            /// @param directory The directory to read.
            /// @return The entries, or an empty vector when directory does not exist or is not a
            /// directory.
            /// @throws std::filesystem::filesystem_error when the file system refuses the path.
            virtual std::vector<DirectoryEntry> read(const std::string& directory) const = 0;
    };

    /// @brief The source the application uses: the file system.
    class FileSystemDirectorySource final : public DirectorySource
    {
        public:
            std::vector<DirectoryEntry> read(const std::string& directory) const override;
    };

    /// @brief A source that serves directories from memory, for tests.
    class InMemoryDirectorySource final : public DirectorySource
    {
        public:
            /// @brief Makes a directory exist with the given entries.
            /// @param directory The directory's name as the caller will pass it.
            /// @param entries Its entries, in any order.
            void add_directory(const std::string& directory, std::vector<DirectoryEntry> entries);

            /// @brief Makes the file system refuse a directory, as it does an overlong path.
            /// @param directory The directory's name as the caller will pass it.
            /// @param cause The error the refusal carries.
            void refuse(const std::string& directory, std::error_code cause);

            /// @brief Reads a directory added with add_directory; a missing one reads empty.
            /// @throws std::filesystem::filesystem_error when the directory was refused.
            std::vector<DirectoryEntry> read(const std::string& directory) const override;

        private:
            std::map<std::string, std::vector<DirectoryEntry>> directories_;
            std::map<std::string, std::error_code> refusals_;
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

    /// @brief Lists a directory read from a given source, keeping the files of several extensions.
    /// @param directory The directory to read.
    /// @param extensions Lowercase extensions to keep; an empty list keeps every file.
    /// @param source Where the entries come from.
    /// @return The entries, ordered as the single-extension listing orders them.
    std::vector<DirectoryEntry> list_directory(const std::string& directory,
                                               const std::vector<std::string>& extensions,
                                               const DirectorySource& source);
} // namespace ui
