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
 * @brief Declares the port through which SushiDSP reads an environment variable.
 * @author Mustafa Garip
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace SushiDSP
{
    /// @brief Reads environment variables for code that must not touch the process environment in
    /// a test. ProcessEnvironment reads the real one; InMemoryEnvironment holds a table.
    class EnvironmentReader
    {
        public:
            EnvironmentReader() = default;
            EnvironmentReader(const EnvironmentReader&) = delete;
            EnvironmentReader& operator=(const EnvironmentReader&) = delete;
            EnvironmentReader(EnvironmentReader&&) = delete;
            EnvironmentReader& operator=(EnvironmentReader&&) = delete;
            virtual ~EnvironmentReader() = default;

            /// @brief Returns the value of the variable @p name, or nothing when it is not set.
            [[nodiscard]] virtual std::optional<std::string> read(std::string_view name) const = 0;
    };
} // namespace SushiDSP
