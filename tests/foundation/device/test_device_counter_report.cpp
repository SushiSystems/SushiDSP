/**************************************************************************/
/* test_device_counter_report.cpp                                         */
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
 * @file test_device_counter_report.cpp
 * @brief Tests that the device report writes its counters as one WARN under the device category.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/device_counter_report.hpp>
#include <SushiDSP/foundation/device/in_memory_block_device.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <chrono>
#include <vector>

namespace
{
    /// @brief Holds a stereo device of 256-frame blocks at 48 kHz.
    constexpr SushiDSP::DeviceFormat K_FORMAT{48000.0, 256, 0, 2};

    /// @brief Serves @p blocks blocks with no callback installed, so each raises no_callback.
    void serve_unanswered_blocks(SushiDSP::InMemoryBlockDevice& device, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            static_cast<void>(device.serve(K_FORMAT.block_size));
        }
    }
} // namespace

TEST(Small_DeviceCounterReport, ADeviceCounterIsWrittenAsOneWarnUnderTheDeviceCategory)
{
    SushiDSP::InMemoryBlockDevice device(K_FORMAT);
    SushiDSP::DeviceCounterReport report;
    report.add_device(device);
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    serve_unanswered_blocks(device, 3);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "device");
    EXPECT_EQ(lines[0].message, "3 no_callback in 5 s");
}

TEST(Small_DeviceCounterReport, NothingMovedWritesNothing)
{
    SushiDSP::InMemoryBlockDevice device(K_FORMAT);
    SushiDSP::DeviceCounterReport report;
    report.add_device(device);
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    EXPECT_TRUE(capture.lines().empty());
}
