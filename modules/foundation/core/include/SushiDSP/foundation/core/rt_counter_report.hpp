/**************************************************************************/
/* rt_counter_report.hpp                                                  */
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
 * @file rt_counter_report.hpp
 * @brief Declares the control-thread reporter that turns moved counters into one line.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <SushiDSP/foundation/core/rt_counter.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace SushiDSP
{
    /**
     * @brief Reads a group of RtCounter objects once per interval and names those that moved.
     *
     * Control thread only. The clock is an argument, so a test drives it without waiting.
     */
    class RtCounterReporter final
    {
        public:
            /// @brief The clock the intervals are read on.
            using Clock = std::chrono::steady_clock;

            /// @brief Builds a reporter that speaks at most once per @p interval.
            explicit RtCounterReporter(Clock::duration interval);

            /// @brief Adds @p counter under @p label; both must outlive the reporter. Only the
            /// events after this call are reported.
            void add(const char* label, const RtCounter& counter);

            /// @brief Adds a count that @p read returns, such as an owner's own atomic; @p label
            /// must outlive the reporter. @p read runs on the control thread.
            void add(const char* label, std::function<std::uint64_t()> read);

            /**
             * @brief Returns the line for the events since the last report, or an empty string.
             *
             * The first call only starts the clock. A later call before the interval has passed
             * returns an empty string, and so does one after it when no counter moved. A line reads
             * `3 no_callback, 1 oversize_block (largest 1024) in 5 s`.
             */
            [[nodiscard]] std::string poll(Clock::time_point now);

        private:
            /// @brief One count and how much of it has been reported.
            struct Entry
            {
                    const char* label;                    /**< The name the line gives it. */
                    std::function<std::uint64_t()> read;  /**< Returns the count. */
                    const RtCounter* counter;             /**< The counter read, or null. */
                    std::uint64_t reported = 0U;          /**< The value at the last report. */
            };

            /// @brief The least time between two reports.
            Clock::duration interval_;

            /// @brief When the last report, or the first poll, happened.
            Clock::time_point last_report_{};

            /// @brief Whether poll has run once.
            bool started_ = false;

            /// @brief The counters, in the order they were added.
            std::vector<Entry> entries_;
    };
} // namespace SushiDSP
