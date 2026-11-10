/**************************************************************************/
/* room_profile_sink.hpp                                                  */
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
 * @file room_profile_sink.hpp
 * @brief Declares the port RoomNode reports its stage zones through and an in-memory adapter that counts them.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace SushiDSP::room
{
    /// @brief Receives the zones RoomNode::process() enters and leaves. A node with no sink reports
    /// to the process-wide profiler through the SD_ZONE macros; a node given one reports to it alone.
    /// Audio thread: an implementation allocates nothing, locks nothing and throws nothing.
    class RoomProfileSink
    {
        public:
            virtual ~RoomProfileSink() = default;

            /// @brief Notes that process() entered a zone.
            /// @param zone One of the names in sushidsp_profile_names.hpp, a string that outlives the node.
            virtual void enter(const char* zone) noexcept = 0;

            /// @brief Notes that process() left the zone it last entered.
            /// @param zone The name passed to the matching enter().
            virtual void leave(const char* zone) noexcept = 0;
    };

    /// @brief Counts how often each zone was entered and left, in a fixed table.
    class InMemoryRoomProfileSink final : public RoomProfileSink
    {
        public:
            /// @brief Counts the distinct zones the table holds; RoomNode reports five.
            static constexpr std::size_t K_ZONE_CAPACITY = 8;

            void enter(const char* zone) noexcept override
            {
                Entry* const entry = find_or_add(zone);
                if (entry != nullptr)
                {
                    ++entry->entered;
                }
            }

            void leave(const char* zone) noexcept override
            {
                Entry* const entry = find_or_add(zone);
                if (entry != nullptr)
                {
                    ++entry->left;
                }
            }

            /// @brief Returns how often a zone was entered.
            /// @param zone The zone's name; compared by text.
            /// @return Zero for a zone never entered.
            [[nodiscard]] std::uint32_t entered(const char* zone) const noexcept
            {
                const Entry* const entry = find(zone);
                return entry == nullptr ? 0u : entry->entered;
            }

            /// @brief Returns how often a zone was left.
            /// @param zone The zone's name; compared by text.
            /// @return Zero for a zone never left.
            [[nodiscard]] std::uint32_t left(const char* zone) const noexcept
            {
                const Entry* const entry = find(zone);
                return entry == nullptr ? 0u : entry->left;
            }

        private:
            /// @brief Holds the counts of one zone.
            struct Entry
            {
                    /// @brief Stores the zone's name; null while the slot is free.
                    const char* name = nullptr;

                    /// @brief Counts the entries.
                    std::uint32_t entered = 0;

                    /// @brief Counts the exits.
                    std::uint32_t left = 0;
            };

            /// @brief Returns the entry named zone, or null.
            [[nodiscard]] const Entry* find(const char* zone) const noexcept
            {
                for (const Entry& entry : entries_)
                {
                    if (entry.name != nullptr && std::strcmp(entry.name, zone) == 0)
                    {
                        return &entry;
                    }
                }
                return nullptr;
            }

            /// @brief Returns the entry named zone, taking a free slot for a new name; null when full.
            Entry* find_or_add(const char* zone) noexcept
            {
                for (Entry& entry : entries_)
                {
                    if (entry.name == nullptr)
                    {
                        entry.name = zone;
                        return &entry;
                    }
                    if (std::strcmp(entry.name, zone) == 0)
                    {
                        return &entry;
                    }
                }
                return nullptr;
            }

            /// @brief Stores one entry per zone seen.
            std::array<Entry, K_ZONE_CAPACITY> entries_{};
    };
} // namespace SushiDSP::room
