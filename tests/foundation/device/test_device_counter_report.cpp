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
 * @file test_device_counter_report.cpp
 * @brief Tests that the device report writes its counters as one WARN under the device category.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/device/device_counter_report.hpp>
#include <SushiDSP/foundation/device/device_counters.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include <chrono>
#include <vector>

namespace
{
    /// @brief A device that offers a counter set and does nothing else.
    class CountingDevice final : public SushiDSP::AudioDevice
    {
        public:
            SushiDSP::DeviceFormat format() const override
            {
                return SushiDSP::DeviceFormat{48000.0, 256, 0, 2};
            }

            void start(SushiDSP::DeviceCallback callback) override
            {
                static_cast<void>(callback);
            }

            void stop() override
            {

            }

            void add_counters_to(SushiDSP::RtCounterReporter& reporter) const override
            {
                counters.add_to(reporter);
            }

            SushiDSP::DeviceCounters counters;
    };
} // namespace

TEST(Small_DeviceCounterReport, ADeviceCounterIsWrittenAsOneWarnUnderTheDeviceCategory)
{
    CountingDevice device;
    SushiDSP::DeviceCounterReport report;
    report.add_device(device);
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    device.counters.overrun.increment(3U);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "device");
    EXPECT_EQ(lines[0].message, "3 overrun in 5 s");
}

TEST(Small_DeviceCounterReport, NothingMovedWritesNothing)
{
    CountingDevice device;
    SushiDSP::DeviceCounterReport report;
    report.add_device(device);
    const SushiDSP::RtCounterReporter::Clock::time_point start{};
    report.poll(start);
    SushiDSP::LogCapture capture;

    report.poll(start + std::chrono::seconds(5));

    EXPECT_TRUE(capture.lines().empty());
}
