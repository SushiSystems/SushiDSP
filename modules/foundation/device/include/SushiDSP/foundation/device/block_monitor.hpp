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
 * @file block_monitor.hpp
 * @brief Declares the watch a device keeps on each block's arrival and processing time.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <SushiDSP/foundation/device/block_clock.hpp>
#include <SushiDSP/foundation/device/device_counters.hpp>
#include <SushiDSP/foundation/device/steady_block_clock.hpp>
#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

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
            /// @brief Builds a monitor that raises @p counters and reads the steady clock.
            explicit BlockMonitor(DeviceCounters& counters) noexcept : counters_(counters), clock_(owned_clock_)
            {

            }

            /// @brief Builds a monitor that raises @p counters and reads @p clock, which must outlive it.
            BlockMonitor(DeviceCounters& counters, const BlockClock& clock) noexcept
                : counters_(counters), clock_(clock)
            {

            }

            /// @brief Notes the arrival of a block of @p frames at @p sample_rate.
            void begin(std::uint32_t frames, double sample_rate) noexcept
            {
                profile::register_this_thread();
                const std::int64_t now = clock_.now_nanoseconds();
                std::uint64_t gap = 0U;
                if (has_previous_ && period_ns_ > 0.0 &&
                    static_cast<double>(now - arrival_) > K_GAP_LIMIT * period_ns_)
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
                    static_cast<double>(clock_.now_nanoseconds() - arrival_) > period_ns_)
                {
                    over = 1U;
                    counters_.overrun.increment();
                }
                SD_COUNTER(profile::names::K_OVERRUN_COUNTER, profile::names::K_COUNT_UNIT, Add,
                           static_cast<double>(over));
            }

        private:
            /// @brief The arrival gap, in block periods, past which a block counts as dropped.
            static constexpr double K_GAP_LIMIT = 1.5;

            /// @brief The nanoseconds in one second.
            static constexpr double K_NANOSECONDS = 1.0e9;

            /// @brief The counters raised.
            DeviceCounters& counters_;

            /// @brief The wall clock the one-argument constructor reads.
            SteadyBlockClock owned_clock_{};

            /// @brief The clock the monitor reads.
            const BlockClock& clock_;

            /// @brief When the last block arrived, in nanoseconds of the clock.
            std::int64_t arrival_ = 0;

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
