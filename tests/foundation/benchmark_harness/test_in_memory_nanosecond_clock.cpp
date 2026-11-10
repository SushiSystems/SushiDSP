/**************************************************************************/
/* test_in_memory_nanosecond_clock.cpp                                    */
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
 * @file test_in_memory_nanosecond_clock.cpp
 * @brief Tests that the in-memory clock starts at zero and moves a fixed step per read.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/in_memory_nanosecond_clock.hpp>

TEST(Small_InMemoryNanosecondClock, TheFirstReadIsZero)
{
    SushiDSP::bench::StepNanosecondClock clock{250.0};

    EXPECT_EQ(clock.now_nanoseconds(), 0.0);
}

TEST(Small_InMemoryNanosecondClock, EachReadAdvancesTheClockByTheStep)
{
    SushiDSP::bench::StepNanosecondClock clock{250.0};

    const double first = clock.now_nanoseconds();
    const double second = clock.now_nanoseconds();

    EXPECT_EQ(second - first, 250.0);
}
