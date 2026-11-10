/**************************************************************************/
/* profile_runtime.cpp                                                    */
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
 * @file profile_runtime.cpp
 * @brief Defines the name, thread and block calls over SushiProfiler's collector.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/profile/profile_runtime.hpp>

#include <SushiDSP/foundation/profile/name_table.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "block_timeline_binding.hpp"

#if SD_ENABLE_PROFILER
#include <SushiProfiler/collector/collector.hpp>
#include <SushiProfiler/periods/period_timeline.hpp>

#include <atomic>
#endif

namespace SushiDSP::profile
{
    namespace
    {
        /// @brief Holds the nanoseconds in one second.
        constexpr double K_NANOSECONDS_PER_SECOND = 1.0e9;
    } // namespace

    std::uint64_t block_budget_ns(std::uint32_t frames, double sample_rate) noexcept
    {
        if (sample_rate <= 0.0)
            return 0U;
        return static_cast<std::uint64_t>(K_NANOSECONDS_PER_SECOND * static_cast<double>(frames) /
                                          sample_rate);
    }

    void mark_block(PeriodMarker& marker, std::uint32_t frames, double sample_rate)
    {
        if (sample_rate <= 0.0)
            return;
        marker.mark(names::K_BLOCK_TIMELINE, block_budget_ns(frames, sample_rate));
    }

#if SD_ENABLE_PROFILER
    namespace
    {
        /// @brief The timeline mark_block opens periods of, or null while no session runs.
        std::atomic<SushiProfiler::PeriodTimeline*> g_block_timeline{nullptr};

        /// @brief Interns through SushiProfiler's collector.
        class CollectorNameTable final : public NameTable
        {
            public:
                /// @brief Returns the collector's id of @p text.
                [[nodiscard]] NameID intern(std::string_view text) override
                {
                    return SushiProfiler::Collector::instance().names().intern(text);
                }
        };
    } // namespace

    NameTable& collector_name_table() noexcept
    {
        static CollectorNameTable table;
        return table;
    }

    NameID intern_name(std::string_view text)
    {
        return collector_name_table().intern(text);
    }

    void register_this_thread()
    {
        thread_local bool registered = false;
        if (registered)
            return;
        registered = true;
        static_cast<void>(SushiProfiler::Collector::register_this_thread());
    }

    void mark_block(std::uint32_t frames, double sample_rate) noexcept
    {
        SushiProfiler::PeriodTimeline* const timeline =
            g_block_timeline.load(std::memory_order_acquire);
        if (timeline == nullptr || sample_rate <= 0.0)
            return;
        static_cast<void>(timeline->mark(block_budget_ns(frames, sample_rate)));
    }

    namespace Detail
    {
        void bind_block_timeline(SushiProfiler::PeriodTimeline* timeline) noexcept
        {
            g_block_timeline.store(timeline, std::memory_order_release);
        }
    } // namespace Detail
#else
    namespace
    {
        /// @brief Interns nothing: this build records nothing.
        class NoNameTable final : public NameTable
        {
            public:
                /// @brief Returns 0 for every text.
                [[nodiscard]] NameID intern(std::string_view text) override
                {
                    static_cast<void>(text);
                    return 0U;
                }
        };
    } // namespace

    NameTable& collector_name_table() noexcept
    {
        static NoNameTable table;
        return table;
    }

    NameID intern_name(std::string_view text)
    {
        return collector_name_table().intern(text);
    }

    void register_this_thread()
    {

    }

    void mark_block(std::uint32_t frames, double sample_rate) noexcept
    {
        static_cast<void>(frames);
        static_cast<void>(sample_rate);
    }

    namespace Detail
    {
        void bind_block_timeline(SushiProfiler::PeriodTimeline* timeline) noexcept
        {
            static_cast<void>(timeline);
        }
    } // namespace Detail
#endif
} // namespace SushiDSP::profile
