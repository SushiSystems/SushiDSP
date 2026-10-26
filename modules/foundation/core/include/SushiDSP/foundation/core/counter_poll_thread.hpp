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
 * @file counter_poll_thread.hpp
 * @brief Declares the thread that polls counter reports for a host whose main thread blocks.
 * @author Mustafa Garip
 * @see modules/foundation/core/README.md
 */

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <utility>

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
            /// @brief Starts calling @p poll; whatever it names must outlive this object.
            explicit CounterPollThread(std::function<void()> poll)
                : poll_(std::move(poll)), thread_([this] { run(); })
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
                thread_.join();
            }

        private:
            /// @brief Calls the function every K_POLL_PERIOD until asked to stop.
            void run()
            {
                while (!stop_.load(std::memory_order_acquire))
                {
                    poll_();
                    std::this_thread::sleep_for(K_POLL_PERIOD);
                }
            }

            /// @brief The time between two calls.
            static constexpr std::chrono::milliseconds K_POLL_PERIOD{500};

            /// @brief The function called.
            std::function<void()> poll_;

            /// @brief Asks the thread to end.
            std::atomic<bool> stop_{false};

            /// @brief The polling thread; declared last so every member above exists before it runs.
            std::thread thread_;
    };
} // namespace SushiDSP
