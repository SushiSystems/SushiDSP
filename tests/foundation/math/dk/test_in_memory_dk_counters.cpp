/**************************************************************************/
/* test_in_memory_dk_counters.cpp                                         */
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
 * @file test_in_memory_dk_counters.cpp
 * @brief Tests that the in-memory DK counters answer what they were given and clear on reset.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/dk/in_memory_dk_counters.hpp>

TEST(Small_InMemoryDkCounters, AnswersEachStatisticItWasGiven)
{
    const SushiDSP::dk::InMemoryDkCounters counters{2048, 2, 7, 3};

    EXPECT_EQ(counters.total_samples(), 2048u);
    EXPECT_EQ(counters.non_converged_samples(), 2u);
    EXPECT_EQ(counters.maximum_iterations_seen(), 7u);
    EXPECT_EQ(counters.nan_fallback_samples(), 3u);
}

TEST(Small_InMemoryDkCounters, ResetStatisticsClearsEveryStatistic)
{
    SushiDSP::dk::InMemoryDkCounters counters{2048, 2, 7, 3};

    counters.reset_statistics();

    EXPECT_EQ(counters.total_samples(), 0u);
    EXPECT_EQ(counters.non_converged_samples(), 0u);
    EXPECT_EQ(counters.maximum_iterations_seen(), 0u);
    EXPECT_EQ(counters.nan_fallback_samples(), 0u);
}
