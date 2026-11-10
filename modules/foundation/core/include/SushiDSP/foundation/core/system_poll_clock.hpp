/**************************************************************************/
/* system_poll_clock.hpp                                                  */
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
 * @file system_poll_clock.hpp
 * @brief Declares the wall-clock PollClock that CounterPollThread uses by default.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include <SushiDSP/foundation/core/poll_clock.hpp>

namespace SushiDSP
{
    /// @brief A PollClock that sleeps half a second per wait, the period of a real host.
    class SystemPollClock final : public PollClock
    {
        public:
            /// @brief Sleeps one period, or returns at once after interrupt().
            void wait_for_next_poll() override
            {
                if (!interrupted_.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(K_POLL_PERIOD);
                }
            }

            /// @brief Makes this and every later wait return at once.
            void interrupt() override
            {
                interrupted_.store(true, std::memory_order_release);
            }

        private:
            /// @brief The time between two polls.
            static constexpr std::chrono::milliseconds K_POLL_PERIOD{500};

            /// @brief Set by interrupt().
            std::atomic<bool> interrupted_{false};
    };
} // namespace SushiDSP
