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
 * @file test_allocation_trap.cpp
 * @brief Tests that AllocationTrap counts the calling thread's allocations inside its window.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

// An unused new-expression may be removed by the optimiser; a direct call to operator new may not.

namespace
{

/** One allocation form the trap promises to count, with the call that releases it. */
struct AllocationForm
{
    const char* name;
    void* (*allocate)(std::size_t);
    void (*release)(void*);
};

void* allocate_single(std::size_t size) { return ::operator new(size); }
void release_single(void* block) { ::operator delete(block); }
void* allocate_array(std::size_t size) { return ::operator new[](size); }
void release_array(void* block) { ::operator delete[](block); }

const AllocationForm kForms[] = {
    {"Single", &allocate_single, &release_single},
    {"Array", &allocate_array, &release_array},
};

} // namespace

class Medium_AllocationTrapForms : public ::testing::TestWithParam<AllocationForm>
{
};

// operator new[] forwards to operator new (allocation_trap.cpp lines 124 to 126), so both forms count once.
TEST_P(Medium_AllocationTrapForms, CountsAnAllocationMadeWhileArmed)
{
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    void* block = GetParam().allocate(64);
    trap.disarm();
    GetParam().release(block);

    EXPECT_EQ(trap.count(), 1);
}

INSTANTIATE_TEST_SUITE_P(Forms, Medium_AllocationTrapForms, ::testing::ValuesIn(kForms),
                         [](const ::testing::TestParamInfo<AllocationForm>& info) { return std::string(info.param.name); });

TEST(Medium_AllocationTrap, IgnoresAllocationsOutsideItsWindow)
{
    SushiDSP::test::AllocationTrap trap;
    std::vector<int> before(32, 1);
    trap.arm();
    trap.disarm();
    std::vector<int> after(32, 2);

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_AllocationTrap, StartsEachTrapFromZero)
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

TEST(Medium_AllocationTrap, ClosesItsWindowWhenDestroyed)
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

TEST(Medium_AllocationTrap, IgnoresAllocationsOfAnotherThread)
{
    std::promise<void> go;
    std::promise<void> done;
    std::shared_future<void> start = go.get_future().share();
    std::future<void> finished = done.get_future();
    std::thread other(
        [start, &done]
        {
            start.wait();
            std::vector<int> values(64, 0);
            done.set_value();
        });

    // The thread and both shared states exist before the window opens, so this thread allocates nothing inside it.
    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    go.set_value();
    finished.wait();
    trap.disarm();
    other.join();

    EXPECT_EQ(trap.count(), 0);
}
