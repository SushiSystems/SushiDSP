/**************************************************************************/
/* counter_poll_thread.hpp                                                */
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
 * @file counter_poll_thread.hpp
 * @brief Declares the thread that polls counter reports for a host whose main thread blocks.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <utility>

#include <SushiDSP/foundation/core/poll_clock.hpp>
#include <SushiDSP/foundation/core/system_poll_clock.hpp>

namespace SushiDSP
{
    /**
     * @brief Calls a function twice a second on a thread of its own until it is destroyed.
     *
     * The function polls the reports of a host. The thread is the control side of the counters:
     * it reads them and logs, and the audio thread never waits for it.
     */
    class CounterPollThread final
    {
        public:
            /// @brief Starts calling @p poll twice a second; whatever it names must outlive this object.
            explicit CounterPollThread(std::function<void()> poll)
                : clock_(owned_clock_), poll_(std::move(poll)), thread_([this] { run(); })
            {

            }

            /// @brief Starts calling @p poll, waiting on @p clock between two calls.
            ///
            /// Both @p poll and @p clock must outlive this object.
            CounterPollThread(std::function<void()> poll, PollClock& clock)
                : clock_(clock), poll_(std::move(poll)), thread_([this] { run(); })
            {

            }

            CounterPollThread(const CounterPollThread&) = delete;
            CounterPollThread& operator=(const CounterPollThread&) = delete;
            CounterPollThread(CounterPollThread&&) = delete;
            CounterPollThread& operator=(CounterPollThread&&) = delete;

            /// @brief Stops the thread and waits for it.
            ~CounterPollThread()
            {
                stop_.store(true, std::memory_order_release);
                clock_.interrupt();
                thread_.join();
            }

        private:
            /// @brief Calls the function, then waits on the clock, until asked to stop.
            void run()
            {
                while (!stop_.load(std::memory_order_acquire))
                {
                    poll_();
                    clock_.wait_for_next_poll();
                }
            }

            /// @brief The wall-clock pacing the one-argument constructor uses.
            SystemPollClock owned_clock_{};

            /// @brief The clock that paces the calls.
            PollClock& clock_;

            /// @brief The function called.
            std::function<void()> poll_;

            /// @brief Asks the thread to end.
            std::atomic<bool> stop_{false};

            /// @brief The polling thread; declared last so every member above exists before it runs.
            std::thread thread_;
    };
} // namespace SushiDSP
