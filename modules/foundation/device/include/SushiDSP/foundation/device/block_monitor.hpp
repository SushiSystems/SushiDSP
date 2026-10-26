/**************************************************************************/
/* block_monitor.hpp                                                      */
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
 * @file block_monitor.hpp
 * @brief Declares the watch a device keeps on each block's arrival and processing time.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <SushiDSP/foundation/device/device_counters.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <chrono>
#include <cstdint>

namespace SushiDSP
{
    /**
     * @brief Marks each arriving block on the `dsp.block` timeline and counts the dropouts.
     *
     * A dropout is an arrival more than 1.5 block periods after the last, or a block whose
     * processing passes its own period. Both are counted in the device's counters and recorded
     * in every block, as 0 or 1, so a record's `counter:` check always has samples. One audio
     * thread calls begin() and end(); nothing here allocates, locks or logs.
     */
    class BlockMonitor final
    {
        public:
            /// @brief Builds a monitor that raises @p counters.
            explicit BlockMonitor(DeviceCounters& counters) noexcept : counters_(counters)
            {

            }

            /// @brief Notes the arrival of a block of @p frames at @p sample_rate.
            void begin(std::uint32_t frames, double sample_rate) noexcept
            {
                profile::register_this_thread();
                const Clock::time_point now = Clock::now();
                std::uint64_t gap = 0U;
                if (has_previous_ && period_ns_ > 0.0 &&
                    std::chrono::duration<double, std::nano>(now - arrival_).count() >
                        K_GAP_LIMIT * period_ns_)
                {
                    gap = 1U;
                    counters_.arrival_gap.increment();
                }
                SD_COUNTER(profile::names::K_ARRIVAL_GAP_COUNTER, profile::names::K_COUNT_UNIT, Add,
                           static_cast<double>(gap));
                profile::mark_block(frames, sample_rate);
                has_previous_ = sample_rate > 0.0;
                arrival_ = now;
                period_ns_ = sample_rate > 0.0 ? K_NANOSECONDS * frames / sample_rate : 0.0;
            }

            /// @brief Notes that the block begun last has been processed.
            void end() noexcept
            {
                std::uint64_t over = 0U;
                if (has_previous_ && period_ns_ > 0.0 &&
                    std::chrono::duration<double, std::nano>(Clock::now() - arrival_).count() >
                        period_ns_)
                {
                    over = 1U;
                    counters_.overrun.increment();
                }
                SD_COUNTER(profile::names::K_OVERRUN_COUNTER, profile::names::K_COUNT_UNIT, Add,
                           static_cast<double>(over));
            }

        private:
            /// @brief The clock the monitor reads.
            using Clock = std::chrono::steady_clock;

            /// @brief The arrival gap, in block periods, past which a block counts as dropped.
            static constexpr double K_GAP_LIMIT = 1.5;

            /// @brief The nanoseconds in one second.
            static constexpr double K_NANOSECONDS = 1.0e9;

            /// @brief The counters raised.
            DeviceCounters& counters_;

            /// @brief When the last block arrived.
            Clock::time_point arrival_{};

            /// @brief The length of the last block's period, in nanoseconds.
            double period_ns_ = 0.0;

            /// @brief Whether a block has arrived yet.
            bool has_previous_ = false;
    };

    /// @brief Calls begin() on construction and end() on destruction, so every return of a
    /// callback is timed.
    class BlockScope final
    {
        public:
            /// @brief Notes the arrival of a block on @p monitor.
            BlockScope(BlockMonitor& monitor, std::uint32_t frames, double sample_rate) noexcept
                : monitor_(monitor)
            {
                monitor_.begin(frames, sample_rate);
            }

            BlockScope(const BlockScope&) = delete;
            BlockScope& operator=(const BlockScope&) = delete;
            BlockScope(BlockScope&&) = delete;
            BlockScope& operator=(BlockScope&&) = delete;

            /// @brief Notes the end of the block's processing.
            ~BlockScope()
            {
                monitor_.end();
            }

        private:
            /// @brief The monitor told when the block ends.
            BlockMonitor& monitor_;
    };
} // namespace SushiDSP
