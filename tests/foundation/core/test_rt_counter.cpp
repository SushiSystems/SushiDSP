/**************************************************************************/
/* test_rt_counter.cpp                                                    */
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
 * @file test_rt_counter.cpp
 * @brief Tests the counter a real-time thread raises.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rt_counter.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

namespace
{
    /// @brief Calls increment() and note() @p rounds times each.
    void raise_both(SushiDSP::RtCounter& counter, std::uint64_t rounds)
    {
        for (std::uint64_t index = 0U; index < rounds; ++index)
        {
            counter.increment();
            counter.note(index);
        }
    }

    /// @brief Calls increment() @p raises times, then sets @p done.
    void raise_then_finish(SushiDSP::RtCounter& counter, std::uint64_t raises, std::atomic<bool>& done)
    {
        for (std::uint64_t index = 0U; index < raises; ++index)
        {
            counter.increment();
        }
        done.store(true, std::memory_order_release);
    }

    /// @brief Reads the counter until @p done is set and returns whether every read was at least the one before.
    bool reads_never_go_backwards(const SushiDSP::RtCounter& counter, const std::atomic<bool>& done)
    {
        bool monotonic = true;
        std::uint64_t last = 0U;
        while (!done.load(std::memory_order_acquire))
        {
            const std::uint64_t value = counter.value();
            monotonic = monotonic && value >= last;
            last = value;
        }
        return monotonic;
    }
} // namespace

TEST(Small_RtCounter, IncrementAddsTheAmountAndValueReadsIt)
{
    SushiDSP::RtCounter counter;
    counter.increment();
    counter.increment(4U);

    EXPECT_EQ(counter.value(), 5U);
}

TEST(Small_RtCounter, IncrementLeavesThePeakUnchanged)
{
    SushiDSP::RtCounter counter;
    counter.increment(4U);

    EXPECT_EQ(counter.peak(), 0U);
}

TEST(Small_RtCounter, NoteCountsOneEventPerCall)
{
    SushiDSP::RtCounter counter;
    counter.note(512U);
    counter.note(2048U);
    counter.note(1024U);

    EXPECT_EQ(counter.value(), 3U);
}

TEST(Small_RtCounter, NoteKeepsTheLargestMagnitude)
{
    SushiDSP::RtCounter counter;
    counter.note(512U);
    counter.note(2048U);
    counter.note(1024U);

    EXPECT_EQ(counter.peak(), 2048U);
}

TEST(Medium_RtCounter, IncrementAndNotePerformNoHeapAllocation)
{
    constexpr std::uint64_t K_ROUNDS = 1000U;
    SushiDSP::RtCounter counter;
    SushiDSP::test::AllocationTrap trap;

    trap.arm();
    raise_both(counter, K_ROUNDS);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_RtCounter, IncrementFromOneThreadWhileAnotherReadsLosesNothing)
{
    constexpr std::uint64_t K_RAISES = 200000U;
    SushiDSP::RtCounter counter;
    std::atomic<bool> done{false};

    std::thread writer(raise_then_finish, std::ref(counter), K_RAISES, std::ref(done));
    const bool monotonic = reads_never_go_backwards(counter, done);
    writer.join();

    EXPECT_TRUE(monotonic);
    EXPECT_EQ(counter.value(), K_RAISES);
}
