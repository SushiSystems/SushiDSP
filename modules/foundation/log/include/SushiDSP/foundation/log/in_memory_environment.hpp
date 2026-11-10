/**************************************************************************/
/* in_memory_environment.hpp                                              */
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
 * @file in_memory_environment.hpp
 * @brief Declares the in-memory EnvironmentReader, which holds a table of variables.
 * @author Mustafa Garip
 */

#pragma once

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SushiDSP/foundation/log/environment_reader.hpp>

namespace SushiDSP
{
    /// @brief An EnvironmentReader over a table held in memory, for tests; it keeps every promise
    /// of EnvironmentReader and touches no process state.
    class InMemoryEnvironment final : public EnvironmentReader
    {
        public:
            /// @brief Holds @p variables, each a name and the value it reads as.
            InMemoryEnvironment(
                std::initializer_list<std::pair<const std::string, std::string>> variables)
                : variables_(variables)
            {

            }

            /// @brief Returns the value held for @p name, or nothing when the table has none.
            [[nodiscard]] std::optional<std::string> read(std::string_view name) const override
            {
                const auto found = variables_.find(std::string(name));
                if (found == variables_.end())
                {
                    return std::nullopt;
                }
                return found->second;
            }

        private:
            /// @brief The variables, by name.
            std::map<std::string, std::string> variables_;
    };
} // namespace SushiDSP
