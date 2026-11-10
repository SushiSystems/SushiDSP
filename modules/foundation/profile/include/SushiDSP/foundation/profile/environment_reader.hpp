/**************************************************************************/
/* environment_reader.hpp                                                 */
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
 * @file environment_reader.hpp
 * @brief Declares the port that reads environment variables, with its two adapters.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace SushiDSP::profile
{
    /// @brief Reads environment variables by name.
    class EnvironmentReader
    {
        public:
            virtual ~EnvironmentReader() = default;

            /// @brief Returns the value of @p name, or an empty string when it is not set.
            [[nodiscard]] virtual std::string value_of(std::string_view name) const = 0;
    };

    /// @brief Answers from a map the caller fills, so no process environment is read.
    class InMemoryEnvironment final : public EnvironmentReader
    {
        public:
            /// @brief Sets variable @p name to @p value.
            void set(std::string name, std::string value)
            {
                values_[std::move(name)] = std::move(value);
            }

            /// @brief Returns the value set for @p name, or an empty string.
            [[nodiscard]] std::string value_of(std::string_view name) const override
            {
                const auto found = values_.find(std::string(name));
                return found == values_.end() ? std::string() : found->second;
            }

        private:
            /// @brief The variables set so far.
            std::map<std::string, std::string> values_;
    };

    /// @brief Returns the reader over the process environment.
    [[nodiscard]] const EnvironmentReader& process_environment() noexcept;
} // namespace SushiDSP::profile
