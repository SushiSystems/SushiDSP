/**************************************************************************/
/* test_node_counters.cpp                                                 */
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
 * @file test_node_counters.cpp
 * @brief Tests the calls that move one block's counts into the process-wide node counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/node_counters.hpp>
#include <SushiDSP/foundation/math/dk/in_memory_dk_counters.hpp>

#include <cstdint>
#include <string>

namespace
{
    /// @brief One clamped block: the samples it cut, and the count the counter must hold after it.
    struct ClampRow
    {
            const char* name;
            std::uint32_t samples;
            std::uint64_t expected;
    };

    constexpr ClampRow K_CLAMP_ROWS[] = {{"TwelveSamples", 12U, 12U}, {"NoSamples", 0U, 0U}};

    /// @brief Statistics of a DK circuit that ran 1024 samples, 3 not converged and 2 fallbacks.
    SushiDSP::dk::InMemoryDkCounters make_statistics()
    {
        return SushiDSP::dk::InMemoryDkCounters(1024U, 3U, 6U, 2U);
    }
} // namespace

class Small_NodeCountersClamp : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_NodeCountersClamp, AClampedBlockRaisesTheOutputCounterByItsSamples)
{
    SushiDSP::NodeCounters counters;

    SushiDSP::note_output_clamp(counters, K_CLAMP_ROWS[GetParam()].samples);

    EXPECT_EQ(counters.output_clamped.value(), K_CLAMP_ROWS[GetParam()].expected);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_NodeCountersClamp, ::testing::Values<std::size_t>(0, 1),
                         [](const ::testing::TestParamInfo<std::size_t>& info) { return std::string(K_CLAMP_ROWS[info.param].name); });

TEST(Small_NodeCounters, NewtonSolvesRaiseTheCapCounterByTheCappedSolves)
{
    SushiDSP::NodeCounters counters;

    SushiDSP::note_newton_solves(counters, 500U, 4U, 1U, 4U);

    EXPECT_EQ(counters.newton_iteration_cap.value(), 4U);
}

TEST(Small_NodeCounters, NewtonSolvesRaiseTheNonFiniteCounterByTheNonFiniteSteps)
{
    SushiDSP::NodeCounters counters;

    SushiDSP::note_newton_solves(counters, 500U, 4U, 1U, 4U);

    EXPECT_EQ(counters.newton_non_finite.value(), 1U);
}

TEST(Small_NodeCounters, ADkFlushMovesTheStatisticsIntoTheCounters)
{
    SushiDSP::NodeCounters counters;
    SushiDSP::dk::InMemoryDkCounters circuit = make_statistics();

    SushiDSP::flush_dk_statistics(counters, circuit);

    EXPECT_EQ(counters.dk_non_converged.value(), 3U);
    EXPECT_EQ(counters.dk_nan_fallback.value(), 2U);
}

TEST(Small_NodeCounters, ADkFlushClearsTheStatisticsOfTheCircuit)
{
    SushiDSP::NodeCounters counters;
    SushiDSP::dk::InMemoryDkCounters circuit = make_statistics();

    SushiDSP::flush_dk_statistics(counters, circuit);

    EXPECT_EQ(circuit.total_samples(), 0U);
    EXPECT_EQ(circuit.non_converged_samples(), 0U);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0U);
}
