/**************************************************************************/
/* DirectoryListing.cpp                                                   */
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

#include "ui/DirectoryListing.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace ui
{
    namespace
    {
        /// @brief Lowercases a string one byte at a time.
        /// @param text The string.
        /// @return The lowercased copy.
        std::string lowercased(const std::string& text)
        {
            std::string out = text;
            std::transform(out.begin(), out.end(), out.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        /// @brief Reports whether a file carries one of the wanted extensions, ignoring case.
        /// @param file The file's path.
        /// @param extensions The extensions, dot included; an empty list matches everything, and
        /// so does an empty entry.
        /// @return True when the file should be listed.
        bool extension_matches(const std::filesystem::path& file,
                               const std::vector<std::string>& extensions)
        {
            if (extensions.empty())
            {
                return true;
            }

            const std::string carried = lowercased(file.extension().string());
            for (const std::string& wanted : extensions)
            {
                if (wanted.empty() || carried == lowercased(wanted))
                {
                    return true;
                }
            }
            return false;
        }

        /// @brief Orders two entries: directories before files, then by name, ignoring case.
        /// @param left The first entry.
        /// @param right The second entry.
        /// @return True when left comes first.
        bool precedes(const DirectoryEntry& left, const DirectoryEntry& right)
        {
            if (left.is_directory != right.is_directory)
            {
                return left.is_directory;
            }
            const std::string left_key = lowercased(left.name);
            const std::string right_key = lowercased(right.name);
            if (left_key != right_key)
            {
                return left_key < right_key;
            }
            return left.name < right.name;
        }
    }

    std::vector<DirectoryEntry> list_directory(const std::string& directory,
                                               const std::string& extension)
    {
        return list_directory(directory, std::vector<std::string>{ extension });
    }

    std::vector<DirectoryEntry> list_directory(const std::string& directory,
                                               const std::vector<std::string>& extensions)
    {
        std::vector<DirectoryEntry> entries;
        try
        {
            const std::filesystem::path root(directory);
            if (!std::filesystem::is_directory(root))
            {
                return entries;
            }
            for (const std::filesystem::directory_entry& item :
                 std::filesystem::directory_iterator(root))
            {
                const bool is_directory = item.is_directory();
                if (!is_directory && !extension_matches(item.path(), extensions))
                {
                    continue;
                }
                entries.push_back(DirectoryEntry{ item.path().filename().string(), is_directory });
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return {};
        }

        std::sort(entries.begin(), entries.end(), precedes);
        return entries;
    }
}
