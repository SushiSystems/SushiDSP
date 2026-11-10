/**************************************************************************/
/* in_memory_poll_clock.hpp                                               */
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
 * @file in_memory_poll_clock.hpp
 * @brief Declares the in-memory PollClock, whose periods the caller grants one at a time.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

#include <SushiDSP/foundation/core/poll_clock.hpp>

namespace SushiDSP
{
    /// @brief A PollClock that waits until tick() grants a period, and never reads the wall clock.
    ///
    /// Each tick() ends one wait. interrupt() ends every wait. wait_until_waiting() lets a test
    /// block until the polling thread has finished a poll and is waiting again, so a test counts
    /// polls without sleeping.
    class InMemoryPollClock final : public PollClock
    {
        public:
            /// @brief Blocks until a period is granted or the clock is interrupted.
            void wait_for_next_poll() override
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ++waits_begun_;
                changed_.notify_all();
                changed_.wait(lock, [this] { return granted_ > 0 || interrupted_; });
                if (granted_ > 0 && !interrupted_)
                {
                    --granted_;
                }
            }

            /// @brief Makes this and every later wait return at once.
            void interrupt() override
            {
                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    interrupted_ = true;
                }
                changed_.notify_all();
            }

            /// @brief Grants one period: one waiting call returns.
            void tick()
            {
                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    ++granted_;
                }
                changed_.notify_all();
            }

            /// @brief Blocks until @p waits waits have begun in all.
            /// @param waits The number of wait_for_next_poll() calls to see begin.
            void wait_until_waiting(std::size_t waits)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock, [this, waits] { return waits_begun_ >= waits; });
            }

        private:
            /// @brief Guards every member below.
            std::mutex mutex_;

            /// @brief Signalled when a wait begins, a period is granted or the clock is interrupted.
            std::condition_variable changed_;

            /// @brief Periods granted and not yet used.
            std::size_t granted_{0};

            /// @brief Waits that have begun since construction.
            std::size_t waits_begun_{0};

            /// @brief Set by interrupt().
            bool interrupted_{false};
    };
} // namespace SushiDSP
