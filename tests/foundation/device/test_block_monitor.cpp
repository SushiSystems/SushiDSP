/**************************************************************************/
/* test_block_monitor.cpp                                                 */
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
 * @file test_block_monitor.cpp
 * @brief Tests that the block monitor counts a late arrival and an overrun and nothing else.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/block_monitor.hpp>
#include <SushiDSP/foundation/device/in_memory_block_clock.hpp>

#include <cstdint>

namespace
{
    /// @brief Holds the frames of an ordinary block.
    constexpr std::uint32_t K_BLOCK = 256;

    /// @brief Holds the sample rate the cases below run at.
    constexpr double K_RATE = 48000.0;

    /// @brief Holds the nanoseconds in one second.
    constexpr std::int64_t K_NANOSECONDS = 1000000000;

    /// @brief Holds the block period of K_BLOCK frames at K_RATE in nanoseconds, rounded up.
    constexpr std::int64_t K_PERIOD_NS = (K_BLOCK * K_NANOSECONDS + 47999) / 48000;

    /// @brief Holds an arrival delay of two periods, past the 1.5 period gap limit.
    constexpr std::int64_t K_LATE_NS = 2 * K_PERIOD_NS;

    /// @brief Holds a processing time of half a period.
    constexpr std::int64_t K_HALF_PERIOD_NS = K_PERIOD_NS / 2;
} // namespace

TEST(Small_BlockMonitor, ABlockThatArrivesMoreThanOneAndAHalfPeriodsLateIsCounted)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);

    monitor.begin(K_BLOCK, K_RATE);
    monitor.end();
    clock.advance(K_LATE_NS);
    monitor.begin(K_BLOCK, K_RATE);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 1U);
}

TEST(Small_BlockMonitor, ABlockThatArrivesInsideItsPeriodCountsNoGap)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);

    monitor.begin(K_BLOCK, K_RATE);
    monitor.end();
    clock.advance(K_PERIOD_NS);
    monitor.begin(K_BLOCK, K_RATE);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 0U);
}

TEST(Small_BlockMonitor, AFirstBlockCountsNoGapWhateverItsSize)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);
    clock.advance(K_LATE_NS);

    monitor.begin(K_BLOCK, K_RATE);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 0U);
}

TEST(Small_BlockMonitor, AnOverrunIsCountedWhenProcessingPassesTheBlocksOwnPeriod)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);

    monitor.begin(K_BLOCK, K_RATE);
    clock.advance(K_LATE_NS);
    monitor.end();

    EXPECT_EQ(counters.overrun.value(), 1U);
}

TEST(Small_BlockMonitor, ABlockProcessedInsideItsPeriodCountsNoOverrun)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);

    monitor.begin(K_BLOCK, K_RATE);
    clock.advance(K_HALF_PERIOD_NS);
    monitor.end();

    EXPECT_EQ(counters.overrun.value(), 0U);
}

TEST(Small_BlockMonitor, TheScopeBeginsOnConstructionAndEndsOnDestruction)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::BlockMonitor monitor(counters, clock);

    {
        const SushiDSP::BlockScope scope(monitor, K_BLOCK, K_RATE);
        clock.advance(K_LATE_NS);
    }

    EXPECT_EQ(counters.overrun.value(), 1U);
}
