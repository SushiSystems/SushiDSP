/**************************************************************************/
/* startup_rig.hpp                                                        */
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
 * @file startup_rig.hpp
 * @brief Declares the reader of the rig preset a plugin starts on when it has no state.
 * @author Mustafa Garip
 */

#pragma once

#include <map>
#include <optional>
#include <string>

namespace SushiDSP::Plugin
{
    /// @brief Names the environment variable that holds the path of the startup rig preset.
    constexpr const char* K_STARTUP_RIG_VARIABLE = "SUSHIDSP_PLUGIN_RIG";

    /// @brief Where the startup rig's text is read from, once its path is known.
    class RigFileReader
    {
        public:
            virtual ~RigFileReader() = default;

            /// @brief Reads a whole file.
            /// @param path The file's path.
            /// @return The file's text, or std::nullopt when the file will not open.
            virtual std::optional<std::string> read(const std::string& path) const = 0;
    };

    /// @brief The reader the plugin uses: the file system.
    class FileRigFileReader final : public RigFileReader
    {
        public:
            std::optional<std::string> read(const std::string& path) const override;
    };

    /// @brief A reader that serves files from memory, for tests; a path it does not hold will not open.
    class InMemoryRigFileReader final : public RigFileReader
    {
        public:
            /// @brief Makes a file exist.
            /// @param path The path the caller will read.
            /// @param text The file's text.
            void add_file(const std::string& path, std::string text);

            std::optional<std::string> read(const std::string& path) const override;

        private:
            std::map<std::string, std::string> files_;
    };

    /// @brief Reads the rig preset a path names, writing one WARN when the file will not open.
    /// @param variable_value The value of K_STARTUP_RIG_VARIABLE, or nullptr when it is unset.
    /// @param reader Where the file's text comes from.
    /// @return The file's text, or std::nullopt when the value is unset or empty, or the file will
    /// not open.
    [[nodiscard]] std::optional<std::string> read_startup_rig(const char* variable_value,
                                                              const RigFileReader& reader);

    /// @brief Reads the rig preset K_STARTUP_RIG_VARIABLE names. Control thread only.
    /// @return The file's text, or std::nullopt when the variable is unset or empty, or the file
    /// will not open.
    [[nodiscard]] std::optional<std::string> read_startup_rig();
} // namespace SushiDSP::Plugin
