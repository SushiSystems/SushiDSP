/**************************************************************************/
/* test_counter_poll_thread.cpp                                           */
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
 * @file test_counter_poll_thread.cpp
 * @brief Tests that the poll thread calls its function once per granted period and stops when destroyed.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/counter_poll_thread.hpp>
#include <SushiDSP/foundation/core/in_memory_poll_clock.hpp>

#include <atomic>

TEST(Medium_CounterPollThread, TheFunctionIsCalledOncePerGrantedPeriod)
{
    std::atomic<int> calls{0};
    SushiDSP::InMemoryPollClock clock;
    {
        SushiDSP::CounterPollThread poller([&] { calls.fetch_add(1); }, clock);

        clock.wait_until_waiting(1);
        EXPECT_EQ(calls.load(), 1);

        clock.tick();
        clock.wait_until_waiting(2);
        EXPECT_EQ(calls.load(), 2);
    }
}

TEST(Medium_CounterPollThread, NoCallHappensAfterTheObjectIsDestroyed)
{
    std::atomic<int> calls{0};
    SushiDSP::InMemoryPollClock clock;
    {
        SushiDSP::CounterPollThread poller([&] { calls.fetch_add(1); }, clock);
        clock.wait_until_waiting(1);
    }
    const int at_destruction = calls.load();
    clock.tick();

    EXPECT_EQ(at_destruction, 1);
    EXPECT_EQ(calls.load(), at_destruction);
}
