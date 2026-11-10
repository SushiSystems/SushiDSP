/**************************************************************************/
/* audio_importer.hpp                                                     */
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
 * @file audio_importer.hpp
 * @brief Declares the port that turns a source file into a WAV, and its in-memory adapter.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <map>
#include <stdexcept>
#include <string>

namespace SushiDSP::host
{
    /// @brief Port that turns a source file into a WAV and names it. AudioImportCache is the
    /// adapter that decodes; AudioImportQueue runs any adapter on its worker thread.
    class AudioImporter
    {
        public:
            virtual ~AudioImporter() = default;

            /// @brief Imports one source file.
            /// @param source_path The file to convert.
            /// @param target_rate The rate to write at, in Hz.
            /// @return The WAV's path.
            /// @throws std::runtime_error when the source cannot be imported.
            virtual std::string import(const std::string& source_path, double target_rate) const = 0;
    };

    /// @brief Adapter that imports nothing from disk: a path imports to that path with ".wav"
    /// appended, unless the caller marked it as failing.
    class InMemoryAudioImporter final : public AudioImporter
    {
        public:
            /// @brief Marks a path as one whose import fails.
            /// @param source_path The file that fails.
            /// @param message The message the failure carries.
            void fail(const std::string& source_path, const std::string& message)
            {
                failures_[source_path] = message;
            }

            std::string import(const std::string& source_path, double) const override
            {
                const auto failure = failures_.find(source_path);
                if (failure != failures_.end())
                {
                    throw std::runtime_error(failure->second);
                }
                return source_path + ".wav";
            }

        private:
            /// @brief Holds the failure message of each failing path.
            std::map<std::string, std::string> failures_;
    };
} // namespace SushiDSP::host
