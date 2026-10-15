/**************************************************************************/
/* test_allocation_trap.cpp                                               */
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
 * @file test_allocation_trap.cpp
 * @brief Tests that AllocationTrap counts the calling thread's allocations inside its window.
 * @author Mustafa Garip
 */

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

// An unused new-expression may be removed by the optimiser; a direct call to operator new may not.

TEST(AllocationTrap, CountsAnAllocationMadeWhileArmed)
{
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    void* block = ::operator new(64);
    trap.disarm();
    ::operator delete(block);

    EXPECT_EQ(trap.count(), 1);
}

TEST(AllocationTrap, CountsAnArrayAllocationMadeWhileArmed)
{
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    void* block = ::operator new[](64);
    trap.disarm();
    ::operator delete[](block);

    EXPECT_EQ(trap.count(), 1);
}

TEST(AllocationTrap, IgnoresAllocationsOutsideItsWindow)
{
    SushiDSP::test::AllocationTrap trap;
    std::vector<int> before(32, 1);
    trap.arm();
    trap.disarm();
    std::vector<int> after(32, 2);

    EXPECT_EQ(trap.count(), 0);
}

TEST(AllocationTrap, StartsEachTrapFromZero)
{
    {
        SushiDSP::test::AllocationTrap first;
        first.arm();
        void* block = ::operator new(16);
        first.disarm();
        ::operator delete(block);
        ASSERT_EQ(first.count(), 1);
    }

    SushiDSP::test::AllocationTrap second;
    EXPECT_EQ(second.count(), 0);
}

TEST(AllocationTrap, ClosesItsWindowWhenDestroyed)
{
    {
        SushiDSP::test::AllocationTrap armed;
        armed.arm();
    }

    SushiDSP::test::AllocationTrap reader;
    void* block = ::operator new(16);
    ::operator delete(block);
    EXPECT_EQ(reader.count(), 0);
}

TEST(AllocationTrap, IgnoresAllocationsOfAnotherThread)
{
    std::atomic<bool> go{false};
    std::atomic<bool> done{false};
    std::thread other(
        [&go, &done]
        {
            while (!go.load())
            {
                std::this_thread::yield();
            }
            std::vector<int> values(64, 0);
            done.store(values.size() == 64);
        });

    // The thread exists before the window opens, so this thread allocates nothing inside it.
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    go.store(true);
    while (!done.load())
    {
        std::this_thread::yield();
    }
    trap.disarm();
    other.join();

    EXPECT_EQ(trap.count(), 0);
}
