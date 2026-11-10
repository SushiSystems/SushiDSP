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
 * @file test_device_counters.cpp
 * @brief Tests that a device's counters reach a reporter under their names.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/device_counters.hpp>

#include <chrono>
#include <cstddef>
#include <string>

namespace
{
    /// @brief One counter of DeviceCounters and the name it reports under.
    struct CounterRow
    {
            const char* name;
            SushiDSP::RtCounter SushiDSP::DeviceCounters::*counter;
    };

    constexpr CounterRow K_COUNTER_ROWS[] = {
        {"no_callback", &SushiDSP::DeviceCounters::no_callback},
        {"oversize_block", &SushiDSP::DeviceCounters::oversize_block},
        {"arrival_gap", &SushiDSP::DeviceCounters::arrival_gap},
        {"overrun", &SushiDSP::DeviceCounters::overrun},
        {"driver_message", &SushiDSP::DeviceCounters::driver_message},
        {"driver_error", &SushiDSP::DeviceCounters::driver_error},
    };
} // namespace

class Small_DeviceCountersByName : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(Small_DeviceCountersByName, EveryCounterIsReportedUnderItsOwnName)
{
    const CounterRow& row = K_COUNTER_ROWS[GetParam()];
    SushiDSP::DeviceCounters counters;
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    counters.add_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));

    (counters.*row.counter).increment(2U);
    const std::string line =
        reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} + std::chrono::seconds(5));

    EXPECT_NE(line.find(std::string("2 ") + row.name), std::string::npos) << line;
}

INSTANTIATE_TEST_SUITE_P(Counters, Small_DeviceCountersByName, ::testing::Values<std::size_t>(0, 1, 2, 3, 4, 5),
                         [](const ::testing::TestParamInfo<std::size_t>& info)
                         { return std::string(K_COUNTER_ROWS[info.param].name); });
