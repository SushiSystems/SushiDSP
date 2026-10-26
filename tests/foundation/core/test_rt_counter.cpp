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
 * @file test_rt_counter.cpp
 * @brief Tests the counter a real-time thread raises.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/rt_counter.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

TEST(Small_RtCounter, IncrementAddsTheAmountAndValueReadsIt)
{
    SushiDSP::RtCounter counter;
    counter.increment();
    counter.increment(4U);

    EXPECT_EQ(counter.value(), 5U);
    EXPECT_EQ(counter.peak(), 0U);
}

TEST(Small_RtCounter, NoteCountsOneEventAndKeepsTheLargestMagnitude)
{
    SushiDSP::RtCounter counter;
    counter.note(512U);
    counter.note(2048U);
    counter.note(1024U);

    EXPECT_EQ(counter.value(), 3U);
    EXPECT_EQ(counter.peak(), 2048U);
}

TEST(Small_RtCounter, IncrementAndNotePerformNoHeapAllocation)
{
    SushiDSP::RtCounter counter;
    SushiDSP::test::AllocationTrap trap;

    trap.arm();
    for (std::uint64_t index = 0U; index < 1000U; ++index)
    {
        counter.increment();
        counter.note(index);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_RtCounter, IncrementFromOneThreadWhileAnotherReadsLosesNothing)
{
    constexpr std::uint64_t K_RAISES = 200000U;
    SushiDSP::RtCounter counter;
    std::atomic<bool> done{false};

    std::thread writer(
        [&]
        {
            for (std::uint64_t index = 0U; index < K_RAISES; ++index)
                counter.increment();
            done.store(true, std::memory_order_release);
        });

    std::uint64_t last = 0U;
    while (!done.load(std::memory_order_acquire))
    {
        const std::uint64_t value = counter.value();
        EXPECT_GE(value, last);
        last = value;
    }
    writer.join();

    EXPECT_EQ(counter.value(), K_RAISES);
}

