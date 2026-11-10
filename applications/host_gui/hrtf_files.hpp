/**************************************************************************/
/* hrtf_files.hpp                                                         */
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
 * @file hrtf_files.hpp
 * @brief Declares the port through which the session reads an HRTF asset, and its two adapters.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <fstream>
#include <ios>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace SushiDSP::host
{
    /// @brief Port to the HRTF assets of a session: the flat file convert_hrtf.py writes is read
    /// from one. DiskHrtfFiles is the adapter that touches the filesystem and InMemoryHrtfFiles
    /// keeps the assets in memory. It is a sibling of AudioFiles, because the asset is not a WAV.
    class HrtfFiles
    {
        public:
            virtual ~HrtfFiles() = default;

            /// @brief Reads the bytes of an asset.
            /// @param path The asset's path.
            /// @param bytes_out Receives the asset's bytes.
            /// @param error_out Receives the reason when the asset cannot be read.
            /// @return False when the asset cannot be read.
            virtual bool read(const std::string& path, std::vector<unsigned char>& bytes_out,
                              std::string& error_out) const = 0;
    };

    /// @brief Adapter that reads the files of the filesystem, byte for byte.
    class DiskHrtfFiles final : public HrtfFiles
    {
        public:
            bool read(const std::string& path, std::vector<unsigned char>& bytes_out,
                      std::string& error_out) const override
            {
                std::ifstream stream(path, std::ios::binary | std::ios::ate);
                if (!stream.is_open())
                {
                    error_out = "cannot open '" + path + "'";
                    return false;
                }
                const std::streampos length = stream.tellg();
                if (length < 0)
                {
                    error_out = "cannot read '" + path + "'";
                    return false;
                }
                bytes_out.resize(static_cast<std::size_t>(length));
                stream.seekg(0, std::ios::beg);
                if (!bytes_out.empty())
                {
                    stream.read(reinterpret_cast<char*>(bytes_out.data()),
                                static_cast<std::streamsize>(bytes_out.size()));
                }
                if (!stream.good() && !stream.eof())
                {
                    error_out = "cannot read '" + path + "'";
                    return false;
                }
                return true;
            }
    };

    /// @brief Returns the adapter a session uses unless it is given another. It holds no state.
    /// @return The one DiskHrtfFiles.
    inline HrtfFiles& disk_hrtf_files()
    {
        static DiskHrtfFiles files;
        return files;
    }

    /// @brief Adapter that holds every asset as a byte vector in a map.
    class InMemoryHrtfFiles final : public HrtfFiles
    {
        public:
            /// @brief Puts an asset in place, as if it had been written before.
            /// @param path The asset's path.
            /// @param bytes The asset's content.
            void add(const std::string& path, std::vector<unsigned char> bytes)
            {
                files_[path] = std::move(bytes);
            }

            bool read(const std::string& path, std::vector<unsigned char>& bytes_out,
                      std::string& error_out) const override
            {
                const auto found = files_.find(path);
                if (found == files_.end())
                {
                    error_out = "cannot open '" + path + "'";
                    return false;
                }
                bytes_out = found->second;
                return true;
            }

        private:
            /// @brief Holds each asset's content by path.
            std::map<std::string, std::vector<unsigned char>> files_;
    };
} // namespace SushiDSP::host
