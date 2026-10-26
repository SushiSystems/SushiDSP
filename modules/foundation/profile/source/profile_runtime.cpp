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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file profile_runtime.cpp
 * @brief Defines the name, thread and block calls over SushiProfiler's collector.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/profile/profile_runtime.hpp>

#include "block_timeline_binding.hpp"

#if SD_ENABLE_PROFILER
#include <SushiProfiler/collector/collector.hpp>
#include <SushiProfiler/periods/period_timeline.hpp>

#include <atomic>
#endif

namespace SushiDSP::profile
{
#if SD_ENABLE_PROFILER
    namespace
    {
        /// @brief The timeline mark_block opens periods of, or null while no session runs.
        std::atomic<SushiProfiler::PeriodTimeline*> g_block_timeline{nullptr};

        /// @brief Holds the nanoseconds in one second.
        constexpr double K_NANOSECONDS_PER_SECOND = 1.0e9;
    } // namespace

    NameID intern_name(std::string_view text)
    {
        return SushiProfiler::Collector::instance().names().intern(text);
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
        const double budget = K_NANOSECONDS_PER_SECOND * static_cast<double>(frames) / sample_rate;
        static_cast<void>(timeline->mark(static_cast<std::uint64_t>(budget)));
    }

    namespace Detail
    {
        void bind_block_timeline(SushiProfiler::PeriodTimeline* timeline) noexcept
        {
            g_block_timeline.store(timeline, std::memory_order_release);
        }
    } // namespace Detail
#else
    NameID intern_name(std::string_view text)
    {
        static_cast<void>(text);
        return 0U;
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
