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

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/solver_counters.hpp>

namespace
{
    /// @brief Declares a circuit whose counters are fixed.
    struct FixedCounters
    {
            std::uint64_t total_samples() const
            {
                return 2048;
            }

            std::uint64_t non_converged_samples() const
            {
                return 2;
            }

            unsigned maximum_iterations_seen() const
            {
                return 7;
            }

            std::uint64_t nan_fallback_samples() const
            {
                return 0;
            }
    };
} // namespace

TEST(Small_SolverCounters, SolverCountersPrintsEveryCounterAndTheNonConvergedShare)
{
    testing::internal::CaptureStdout();
    SushiDSP::bench::print_solver_counters(FixedCounters{});
    const std::string line = testing::internal::GetCapturedStdout();

    EXPECT_EQ(line, "  solver: 2048 solves, 2 non-converged (0.0977 %), 7 iterations worst case, "
                    "0 NaN fallbacks\n");
}
