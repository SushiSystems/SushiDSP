/**************************************************************************/
/* process_environment.hpp                                                */
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
 * @file process_environment.hpp
 * @brief Declares the EnvironmentReader over the environment of the process.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include <SushiDSP/foundation/log/environment_reader.hpp>

namespace SushiDSP
{
    /// @brief The EnvironmentReader the product uses: it reads the environment of the process.
    class ProcessEnvironment final : public EnvironmentReader
    {
        public:
            /// @brief Returns the value of the process variable @p name, or nothing when unset.
            [[nodiscard]] std::optional<std::string> read(std::string_view name) const override
            {
                const char* const value = std::getenv(std::string(name).c_str());
                if (value == nullptr)
                {
                    return std::nullopt;
                }
                return std::string(value);
            }
    };
} // namespace SushiDSP
