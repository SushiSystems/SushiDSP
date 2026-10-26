/**************************************************************************/
/* test_device_counters.cpp                                               */
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
 * @file test_device_counters.cpp
 * @brief Tests that a device's counters reach a reporter under their names.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/device_counters.hpp>

#include <chrono>
#include <string>

TEST(Small_DeviceCounters, EveryCounterIsReportedUnderItsOwnName)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    counters.add_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));

    counters.no_callback.increment();
    counters.oversize_block.note(1024U);
    counters.arrival_gap.increment(2U);
    counters.overrun.increment();
    counters.driver_message.increment();
    counters.driver_error.increment();
    const std::string line =
        reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} + std::chrono::seconds(5));

    EXPECT_EQ(line,
              "1 no_callback, 1 oversize_block (largest 1024), 2 arrival_gap, 1 overrun, "
              "1 driver_message, 1 driver_error in 5 s");
}

TEST(Small_DeviceCounters, ACleanDeviceReportsNothing)
{
    SushiDSP::DeviceCounters counters;
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    counters.add_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));

    EXPECT_TRUE(
        reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} + std::chrono::seconds(5))
            .empty());
}
