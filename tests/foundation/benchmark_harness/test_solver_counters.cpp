/**************************************************************************/
/* test_solver_counters.cpp                                               */
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
 * @file test_solver_counters.cpp
 * @brief Tests the counter line print_solver_counters() writes under a solver row.
 * @author Mustafa Garip
 */

#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/solver_counters.hpp>
#include <SushiDSP/foundation/math/dk/in_memory_dk_counters.hpp>

namespace
{
    /// @brief Prints the counters and returns the line they wrote.
    std::string printed_line(const SushiDSP::dk::InMemoryDkCounters& counters)
    {
        testing::internal::CaptureStdout();
        SushiDSP::bench::print_solver_counters(counters);
        return testing::internal::GetCapturedStdout();
    }
} // namespace

// 2 of 2048 solves is 0.09765625 percent, which prints as 0.0977.
TEST(Small_SolverCounters, SolverCountersPrintsEveryCounterAndTheNonConvergedShare)
{
    const std::string line = printed_line(SushiDSP::dk::InMemoryDkCounters{2048, 2, 7, 0});

    EXPECT_EQ(line, "  solver: 2048 solves, 2 non-converged (0.0977 %), 7 iterations worst case, "
                    "0 NaN fallbacks\n");
}

TEST(Small_SolverCounters, SolverCountersPrintsAZeroShareForACircuitWithNoSolves)
{
    const std::string line = printed_line(SushiDSP::dk::InMemoryDkCounters{0, 0, 0, 0});

    EXPECT_EQ(line, "  solver: 0 solves, 0 non-converged (0.0000 %), 0 iterations worst case, "
                    "0 NaN fallbacks\n");
}
