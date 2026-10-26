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
 * @file test_block_monitor.cpp
 * @brief Tests that the block monitor counts a late arrival and an overrun and nothing else.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/block_monitor.hpp>

#include <chrono>
#include <thread>

namespace
{
    /// @brief Holds a pause far longer than the 5.33 ms block period the cases below use.
    constexpr std::chrono::milliseconds K_LONG_PAUSE{40};

    /// @brief Holds frames that make one block last a whole second at 48 kHz.
    constexpr std::uint32_t K_SECOND_OF_FRAMES = 48000;

    /// @brief Holds the frames of an ordinary block.
    constexpr std::uint32_t K_BLOCK = 256;
} // namespace

TEST(Medium_BlockMonitor, ABlockThatArrivesMoreThanOneAndAHalfPeriodsLateIsCounted)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    monitor.begin(K_BLOCK, 48000.0);
    monitor.end();
    std::this_thread::sleep_for(K_LONG_PAUSE);
    monitor.begin(K_BLOCK, 48000.0);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 1U);
}

TEST(Medium_BlockMonitor, ABlockThatArrivesInsideItsPeriodCountsNoGap)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    monitor.begin(K_SECOND_OF_FRAMES, 48000.0);
    monitor.end();
    monitor.begin(K_SECOND_OF_FRAMES, 48000.0);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 0U);
}

TEST(Medium_BlockMonitor, AFirstBlockCountsNoGapWhateverItsSize)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    monitor.begin(K_BLOCK, 48000.0);
    monitor.end();

    EXPECT_EQ(counters.arrival_gap.value(), 0U);
}

TEST(Medium_BlockMonitor, AnOverrunIsCountedWhenProcessingPassesTheBlocksOwnPeriod)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    monitor.begin(K_BLOCK, 48000.0);
    std::this_thread::sleep_for(K_LONG_PAUSE);
    monitor.end();

    EXPECT_EQ(counters.overrun.value(), 1U);
}

TEST(Medium_BlockMonitor, ABlockProcessedInsideItsPeriodCountsNoOverrun)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    monitor.begin(K_SECOND_OF_FRAMES, 48000.0);
    monitor.end();

    EXPECT_EQ(counters.overrun.value(), 0U);
}

TEST(Medium_BlockMonitor, TheScopeBeginsOnConstructionAndEndsOnDestruction)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::BlockMonitor monitor(counters);

    {
        const SushiDSP::BlockScope scope(monitor, K_BLOCK, 48000.0);
        std::this_thread::sleep_for(K_LONG_PAUSE);
    }

    EXPECT_EQ(counters.overrun.value(), 1U);
}
