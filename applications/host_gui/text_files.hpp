/**************************************************************************/
/* text_files.hpp                                                         */
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
 * @file text_files.hpp
 * @brief Declares the port through which the session reads and writes its text files, and its two
 * adapters.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <fstream>
#include <ios>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace SushiDSP::host
{
    /// @brief Port to the text files of a session: the project file and a track's preset.
    /// DiskTextFiles is the adapter that touches the filesystem and InMemoryTextFiles keeps the
    /// files in memory.
    class TextFiles
    {
        public:
            virtual ~TextFiles() = default;

            /// @brief Reads a whole text file.
            /// @param path The file's path.
            /// @param text_out Receives the file's bytes.
            /// @param error_out Receives the reason when the file cannot be opened.
            /// @return False when the file cannot be opened.
            virtual bool read(const std::string& path, std::string& text_out,
                              std::string& error_out) const = 0;

            /// @brief Writes a whole text file, replacing what is there.
            /// @param path The file's path.
            /// @param text The bytes to write.
            /// @param error_out Receives the reason when the file cannot be opened for writing.
            /// @return False when the file cannot be written.
            virtual bool write(const std::string& path, const std::string& text,
                               std::string& error_out) = 0;
    };

    /// @brief Adapter that reads and writes the files of the filesystem, byte for byte.
    class DiskTextFiles final : public TextFiles
    {
        public:
            bool read(const std::string& path, std::string& text_out,
                      std::string& error_out) const override
            {
                std::ifstream in(path, std::ios::binary);
                if (!in)
                {
                    error_out = "Cannot open " + path + ".";
                    return false;
                }
                std::ostringstream buffer;
                buffer << in.rdbuf();
                text_out = buffer.str();
                return true;
            }

            bool write(const std::string& path, const std::string& text,
                       std::string& error_out) override
            {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    error_out = "Cannot write " + path + ".";
                    return false;
                }
                out << text;
                return static_cast<bool>(out);
            }
    };

    /// @brief Returns the adapter a session uses unless it is given another. It holds no state.
    /// @return The one DiskTextFiles.
    inline TextFiles& disk_text_files()
    {
        static DiskTextFiles files;
        return files;
    }

    /// @brief Adapter that holds every file as a string in a map.
    class InMemoryTextFiles final : public TextFiles
    {
        public:
            /// @brief Puts a file in place, as if it had been written before.
            /// @param path The file's path.
            /// @param text The file's content.
            void add(const std::string& path, const std::string& text)
            {
                files_[path] = text;
            }

            /// @brief Makes every later write to a path fail, as a read-only folder does.
            /// @param path The path whose write fails.
            void refuse_writing(const std::string& path)
            {
                refused_.insert(path);
            }

            /// @brief Reports whether a file exists.
            /// @param path The file's path.
            bool exists(const std::string& path) const
            {
                return files_.count(path) != 0u;
            }

            bool read(const std::string& path, std::string& text_out,
                      std::string& error_out) const override
            {
                const auto found = files_.find(path);
                if (found == files_.end())
                {
                    error_out = "Cannot open " + path + ".";
                    return false;
                }
                text_out = found->second;
                return true;
            }

            bool write(const std::string& path, const std::string& text,
                       std::string& error_out) override
            {
                if (refused_.count(path) != 0u)
                {
                    error_out = "Cannot write " + path + ".";
                    return false;
                }
                files_[path] = text;
                return true;
            }

        private:
            /// @brief Holds each file's content by path.
            std::map<std::string, std::string> files_;

            /// @brief Holds the paths whose write fails.
            std::set<std::string> refused_;
    };
} // namespace SushiDSP::host
