/**************************************************************************/
/* source_stat.hpp                                                        */
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
 * @file source_stat.hpp
 * @brief Declares the port that reports a source file's size and write time, and its two adapters.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>

namespace SushiDSP::host
{
    /// @brief What a source file looked like when it was examined.
    struct SourceStat
    {
        /// @brief The size in bytes; zero when the file cannot be examined.
        unsigned long long bytes = 0;

        /// @brief The last write time in the filesystem's ticks; zero when the file cannot be examined.
        long long stamp = 0;
    };

    /// @brief Port that reports a source file's size and last write time.
    class SourceStatReader
    {
        public:
            virtual ~SourceStatReader() = default;

            /// @brief Examines a file.
            /// @param path The file's path.
            /// @return Its size and write time, both zero when it cannot be examined.
            virtual SourceStat stat(const std::string& path) const = 0;
    };

    /// @brief Adapter that reads the size and write time from the filesystem.
    class FileSourceStatReader final : public SourceStatReader
    {
        public:
            SourceStat stat(const std::string& path) const override
            {
                const std::filesystem::path file(path);

                std::error_code size_failure;
                const std::uintmax_t size = std::filesystem::file_size(file, size_failure);

                std::error_code time_failure;
                const std::filesystem::file_time_type written =
                    std::filesystem::last_write_time(file, time_failure);

                SourceStat result;
                result.bytes = size_failure ? 0ull : static_cast<unsigned long long>(size);
                result.stamp =
                    time_failure ? 0ll : static_cast<long long>(written.time_since_epoch().count());
                return result;
            }
    };

    /// @brief Adapter that answers from a table the caller fills; a path not in the table reads as missing.
    class InMemorySourceStatReader final : public SourceStatReader
    {
        public:
            /// @brief Records what a path will report.
            /// @param path The file's path.
            /// @param value The size and write time it reports.
            void set(const std::string& path, SourceStat value)
            {
                table_[path] = value;
            }

            SourceStat stat(const std::string& path) const override
            {
                const auto found = table_.find(path);
                return found == table_.end() ? SourceStat{} : found->second;
            }

        private:
            /// @brief Holds the reported value of each path.
            std::map<std::string, SourceStat> table_;
    };
} // namespace SushiDSP::host
