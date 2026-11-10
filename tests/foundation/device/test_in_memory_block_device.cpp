/**************************************************************************/
/* test_in_memory_block_device.cpp                                        */
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
 * @file test_in_memory_block_device.cpp
 * @brief Tests that the in-memory block device keeps the promises of AudioDevice and serves blocks on demand.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/in_memory_block_clock.hpp>
#include <SushiDSP/foundation/device/in_memory_block_device.hpp>
#include <SushiDSP/foundation/profile/profile_runtime.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds a stereo device of 256-frame blocks at 48 kHz.
    constexpr SushiDSP::DeviceFormat K_FORMAT{48000.0, 256, 0, 2};

    /// @brief Holds the level the callback writes to every sample.
    constexpr float K_LEVEL = 0.5f;

    /// @brief Holds the block period of K_FORMAT in nanoseconds, rounded up.
    constexpr std::int64_t K_PERIOD_NS = (256LL * 1000000000LL + 47999) / 48000;

    /// @brief Holds the warm-up periods of the timeline window.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the timeline window.
    constexpr std::uint32_t K_ACTIVE = 20;

    /// @brief Serves @p blocks blocks, one period apart on @p clock.
    void serve_periodically(SushiDSP::InMemoryBlockDevice& device, SushiDSP::InMemoryBlockClock& clock, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            static_cast<void>(device.serve(K_FORMAT.block_size));
            clock.advance(K_PERIOD_NS);
        }
    }

    /// @brief Returns whether every sample of @p stream equals @p expected.
    bool all_equal(const std::vector<float>& stream, float expected)
    {
        return stream == std::vector<float>(stream.size(), expected);
    }
} // namespace

/// @brief Skips the timeline test in a build that records nothing.
class Medium_InMemoryBlockDeviceTimeline : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            if (!SushiDSP::profile::K_PROFILER_BUILT)
            {
                GTEST_SKIP() << "this build records nothing";
            }
        }
};

TEST(Small_InMemoryBlockDevice, FormatIsTheFormatItWasMadeWith)
{
    const SushiDSP::InMemoryBlockDevice device(K_FORMAT);

    EXPECT_EQ(device.format().block_size, K_FORMAT.block_size);
    EXPECT_EQ(device.format().output_channels, K_FORMAT.output_channels);
}

TEST(Small_InMemoryBlockDevice, AStartedDeviceServesTheCallbacksOutput)
{
    SushiDSP::InMemoryBlockDevice device(K_FORMAT);
    device.start([](const float* const*, float* const* outputs, std::uint32_t frames)
                 {
                     for (std::uint32_t channel = 0; channel < 2; ++channel)
                     {
                         std::fill(outputs[channel], outputs[channel] + frames, K_LEVEL);
                     }
                 });

    EXPECT_TRUE(all_equal(device.serve(K_FORMAT.block_size), K_LEVEL));
}

TEST(Small_InMemoryBlockDevice, AStoppedDeviceServesSilence)
{
    SushiDSP::InMemoryBlockDevice device(K_FORMAT);
    device.start([](const float* const*, float* const*, std::uint32_t) {});
    device.stop();

    EXPECT_TRUE(all_equal(device.serve(K_FORMAT.block_size), 0.0f));
}

TEST(Small_InMemoryBlockDevice, TheDeviceAddsItsCountersToAReporterByName)
{
    SushiDSP::InMemoryBlockDevice device(K_FORMAT);
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    device.add_counters_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));

    static_cast<void>(device.serve(K_FORMAT.block_size));
    const std::string line =
        reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} + std::chrono::seconds(5));

    EXPECT_EQ(line, "1 no_callback in 5 s");
}

TEST_F(Medium_InMemoryBlockDeviceTimeline, BlocksOpenPeriodsOnTheBlockTimelineWithOneBlockOfBudget)
{
    SushiDSP::InMemoryBlockClock clock;
    SushiDSP::InMemoryBlockDevice device(K_FORMAT, clock);
    device.start([](const float* const*, float* const*, std::uint32_t) {});
    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BLOCK_TIMELINE, K_WARMUP, K_ACTIVE)
                    .started());

    serve_periodically(device, clock, K_WARMUP + K_ACTIVE + 3);
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    const SushiDSP::profile::TimelineSummary summary =
        finished.record.summarise(SushiDSP::profile::names::K_BLOCK_TIMELINE);
    EXPECT_EQ(summary.periods, K_ACTIVE);
    EXPECT_NEAR(summary.budget_ns, 1.0e9 * K_FORMAT.block_size / K_FORMAT.sample_rate, 1.0);
    const SushiDSP::profile::ProfileSubject* const gaps = finished.record.find("counter:device.arrival_gap");
    ASSERT_NE(gaps, nullptr);
    EXPECT_EQ(gaps->samples.size(), K_ACTIVE);
}
