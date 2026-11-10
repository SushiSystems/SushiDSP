/**************************************************************************/
/* in_memory_golden_store.hpp                                             */
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
 * @file in_memory_golden_store.hpp
 * @brief Declares the GoldenStore that keeps its reference samples in memory.
 * @author Mustafa Garip
 * @see modules/foundation/golden_compare/README.md
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <SushiDSP/foundation/golden_compare/golden_store.hpp>

namespace SushiDSP::golden
{
    /// @brief A GoldenStore held in memory: it starts empty and keeps what write() gives it.
    class InMemoryGoldenStore final : public GoldenStore
    {
        public:
            /// @brief Reads up to @p wanted samples of the entry at @p path; false when there is none.
            [[nodiscard]] bool read(const std::string& path, std::vector<float>& samples,
                                    std::size_t wanted) const override
            {
                const auto entry = entries_.find(path);
                if (entry == entries_.end())
                {
                    return false;
                }
                samples.assign(entry->second.begin(),
                               entry->second.begin() + static_cast<std::ptrdiff_t>(std::min(wanted, entry->second.size())));
                return true;
            }

            /// @brief Keeps @p samples under @p path, replacing the entry there.
            [[nodiscard]] bool write(const std::string& path, const std::vector<float>& samples) override
            {
                entries_[path] = samples;
                return true;
            }

            /// @brief Returns whether an entry exists at @p path.
            [[nodiscard]] bool contains(const std::string& path) const
            {
                return entries_.count(path) != 0;
            }

        private:
            /// @brief The entries by path.
            std::map<std::string, std::vector<float>> entries_;
    };
} // namespace SushiDSP::golden
