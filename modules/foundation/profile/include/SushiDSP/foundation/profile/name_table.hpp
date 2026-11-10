/**************************************************************************/
/* name_table.hpp                                                         */
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
 * @file name_table.hpp
 * @brief Declares the port that interns zone and lane names, with its two adapters.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_runtime.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

namespace SushiDSP::profile
{
    /// @brief Gives each distinct text one id; the empty text has none.
    class NameTable
    {
        public:
            virtual ~NameTable() = default;

            /// @brief Returns the id of @p text, interning it on first use; 0 for the empty text.
            [[nodiscard]] virtual NameID intern(std::string_view text) = 0;
    };

    /// @brief Numbers the texts it is given from 1, in order of first use.
    class InMemoryNameTable final : public NameTable
    {
        public:
            /// @brief Returns the id of @p text, 0 for the empty text.
            [[nodiscard]] NameID intern(std::string_view text) override
            {
                if (text.empty())
                    return 0U;
                const auto found = ids_.find(std::string(text));
                if (found != ids_.end())
                    return found->second;
                const NameID id = static_cast<NameID>(ids_.size()) + 1U;
                ids_.emplace(std::string(text), id);
                return id;
            }

        private:
            /// @brief The id of each text interned so far.
            std::unordered_map<std::string, NameID> ids_;
    };

    /// @brief Returns the table over SushiProfiler's collector, which intern_name() uses.
    [[nodiscard]] NameTable& collector_name_table() noexcept;
} // namespace SushiDSP::profile
