/**************************************************************************/
/* rt_counter.hpp                                                         */
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
 * @file rt_counter.hpp
 * @brief Declares the counter a real-time thread raises where it may not log.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace SushiDSP
{
    /**
     * @brief Counts events on a real-time thread so that another thread can report them.
     *
     * A raise is one relaxed atomic read-modify-write: no allocation, no lock, no system call.
     * Any thread may read. The audio thread never logs; the control thread reads the counter and
     * writes one line per interval through an RtCounterReporter.
     */
    class RtCounter final
    {
        public:
            /// @brief Builds a counter at zero.
            RtCounter() noexcept = default;

            RtCounter(const RtCounter&) = delete;
            RtCounter& operator=(const RtCounter&) = delete;
            RtCounter(RtCounter&&) = delete;
            RtCounter& operator=(RtCounter&&) = delete;
            ~RtCounter() = default;

            /// @brief Adds @p amount events.
            void increment(std::uint64_t amount = 1U) noexcept
            {
                count_.fetch_add(amount, std::memory_order_relaxed);
            }

            /// @brief Adds one event of size @p magnitude and keeps the largest size seen.
            void note(std::uint64_t magnitude) noexcept
            {
                count_.fetch_add(1U, std::memory_order_relaxed);
                std::uint64_t seen = peak_.load(std::memory_order_relaxed);
                while (magnitude > seen &&
                       !peak_.compare_exchange_weak(seen, magnitude, std::memory_order_relaxed))
                {

                }
            }

            /// @brief Returns the events counted so far.
            [[nodiscard]] std::uint64_t value() const noexcept
            {
                return count_.load(std::memory_order_relaxed);
            }

            /// @brief Returns the largest magnitude passed to note(), or 0.
            [[nodiscard]] std::uint64_t peak() const noexcept
            {
                return peak_.load(std::memory_order_relaxed);
            }

        private:
            /// @brief The events counted.
            std::atomic<std::uint64_t> count_{0U};

            /// @brief The largest magnitude noted.
            std::atomic<std::uint64_t> peak_{0U};
    };

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "a real-time counter must never fall back to a lock");
} // namespace SushiDSP
