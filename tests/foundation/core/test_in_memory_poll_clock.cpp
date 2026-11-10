/**************************************************************************/
/* test_in_memory_poll_clock.cpp                                          */
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
 * @file test_in_memory_poll_clock.cpp
 * @brief Tests that the in-memory PollClock ends a wait on a granted period or an interrupt, and not before.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_poll_clock.hpp>

#include <atomic>
#include <functional>
#include <thread>

namespace
{
    /// @brief Waits on @p clock @p count times, adding one to @p completed after each wait returns.
    void wait_repeatedly(SushiDSP::InMemoryPollClock& clock, int count, std::atomic<int>& completed)
    {
        for (int index = 0; index < count; ++index)
        {
            clock.wait_for_next_poll();
            completed.fetch_add(1);
        }
    }
} // namespace

TEST(Small_InMemoryPollClock, AGrantedPeriodEndsOneWait)
{
    SushiDSP::InMemoryPollClock clock;
    std::atomic<int> completed{0};
    std::thread waiter(wait_repeatedly, std::ref(clock), 1, std::ref(completed));
    clock.wait_until_waiting(1);

    clock.tick();
    waiter.join();

    EXPECT_EQ(completed.load(), 1);
}

TEST(Small_InMemoryPollClock, APeriodGrantedBeforeTheWaitIsNotLost)
{
    SushiDSP::InMemoryPollClock clock;
    std::atomic<int> completed{0};
    clock.tick();

    std::thread waiter(wait_repeatedly, std::ref(clock), 1, std::ref(completed));
    waiter.join();

    EXPECT_EQ(completed.load(), 1);
}

TEST(Small_InMemoryPollClock, OneGrantedPeriodDoesNotEndASecondWait)
{
    SushiDSP::InMemoryPollClock clock;
    std::atomic<int> completed{0};
    std::thread waiter(wait_repeatedly, std::ref(clock), 2, std::ref(completed));
    clock.wait_until_waiting(1);

    clock.tick();
    clock.wait_until_waiting(2);
    const int after_one_period = completed.load();
    clock.interrupt();
    waiter.join();

    EXPECT_EQ(after_one_period, 1);
}

TEST(Small_InMemoryPollClock, AnInterruptEndsEveryWait)
{
    SushiDSP::InMemoryPollClock clock;
    std::atomic<int> completed{0};
    clock.interrupt();

    std::thread waiter(wait_repeatedly, std::ref(clock), 3, std::ref(completed));
    waiter.join();

    EXPECT_EQ(completed.load(), 3);
}
