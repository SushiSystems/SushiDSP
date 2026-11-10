/**************************************************************************/
/* period_marker.hpp                                                      */
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
 * @file period_marker.hpp
 * @brief Declares the port that opens the periods of a timeline, and its in-memory adapter.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SushiDSP::profile
{
    /// @brief Opens the periods of a timeline; ProfileSession is the one that records them.
    class PeriodMarker
    {
        public:
            virtual ~PeriodMarker() = default;

            /// @brief Opens the next period of @p timeline with @p budget_ns, 0 for none.
            virtual void mark(std::string_view timeline, std::uint64_t budget_ns) = 0;
    };

    /// @brief One period an InMemoryPeriodMarker was asked to open.
    struct MarkedPeriod
    {
            std::string timeline;      /**< The timeline the period belongs to. */
            std::uint64_t budget_ns{}; /**< The budget the period was opened with. */
    };

    /// @brief Keeps every mark in order, so a test reads what a caller asked for.
    class InMemoryPeriodMarker final : public PeriodMarker
    {
        public:
            /// @brief Appends the period to marks().
            void mark(std::string_view timeline, std::uint64_t budget_ns) override
            {
                marks_.push_back(MarkedPeriod{std::string(timeline), budget_ns});
            }

            /// @brief Returns the periods opened so far, oldest first.
            [[nodiscard]] const std::vector<MarkedPeriod>& marks() const noexcept
            {
                return marks_;
            }

        private:
            /// @brief The periods opened so far.
            std::vector<MarkedPeriod> marks_;
    };
} // namespace SushiDSP::profile
