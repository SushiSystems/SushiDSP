/**************************************************************************/
/* test_sdl2_audio_device.cpp                                             */
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
 * @file test_sdl2_audio_device.cpp
 * @brief Tests the SDL2 device's counters and block timeline against SDL2's dummy audio driver.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/sdl2_audio_device.hpp>
#include <SushiDSP/foundation/profile/profile_session.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

namespace
{
    /// @brief The longest a case waits for blocks to arrive before it gives up; it is a bound on a
    /// wait, never a time a case asserts.
    constexpr std::chrono::seconds K_WAIT_BOUND{10};

    /// @brief Points SDL2 at its dummy driver, which has no hardware and paces blocks itself.
    void use_dummy_driver()
    {
#if defined(_WIN32)
        _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
        setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
    }

    /// @brief Waits until @p done is true or the bound passes; returns whether it became true.
    template <typename Done> bool wait_until(Done&& done)
    {
        const auto deadline = std::chrono::steady_clock::now() + K_WAIT_BOUND;
        while (!done())
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return true;
    }
} // namespace

TEST(Medium_Sdl2AudioDevice, ABlockWithNoCallbackInstalledIsCountedAndAnswered)
{
    use_dummy_driver();
    SushiDSP::Sdl2AudioDevice device(SushiDSP::DeviceFormat{48000.0, 256, 0, 2});

    device.start(SushiDSP::DeviceCallback{});
    const bool counted = wait_until([&] { return device.counters().no_callback.value() > 0U; });
    device.stop();

    EXPECT_TRUE(counted);
}

TEST(Medium_Sdl2AudioDevice, ABlockRunsTheCallbackWithItsFramesAndCountsNoMissingCallback)
{
    use_dummy_driver();
    SushiDSP::Sdl2AudioDevice device(SushiDSP::DeviceFormat{48000.0, 256, 0, 2});
    std::atomic<std::uint32_t> blocks{0};
    std::atomic<std::uint32_t> frames_seen{0};

    device.start(
        [&](const float* const*, float* const*, std::uint32_t frames)
        {
            frames_seen.store(frames);
            blocks.fetch_add(1);
        });
    const bool ran = wait_until([&] { return blocks.load() >= 5U; });
    device.stop();

    ASSERT_TRUE(ran);
    EXPECT_EQ(frames_seen.load(), device.format().block_size);
    EXPECT_EQ(device.counters().no_callback.value(), 0U);
    EXPECT_EQ(device.counters().oversize_block.value(), 0U);
}

TEST(Medium_Sdl2AudioDevice, TheDeviceAddsItsCountersToAReporterByName)
{
    use_dummy_driver();
    SushiDSP::Sdl2AudioDevice device(SushiDSP::DeviceFormat{48000.0, 256, 0, 2});
    SushiDSP::RtCounterReporter reporter(std::chrono::seconds(5));
    device.add_counters_to(reporter);
    static_cast<void>(reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{}));

    device.start(SushiDSP::DeviceCallback{});
    const bool counted = wait_until([&] { return device.counters().no_callback.value() > 0U; });
    device.stop();
    const std::string line =
        reporter.poll(SushiDSP::RtCounterReporter::Clock::time_point{} + std::chrono::seconds(5));

    ASSERT_TRUE(counted);
    EXPECT_NE(line.find("no_callback"), std::string::npos) << line;
}

TEST(Medium_Sdl2AudioDevice, BlocksOpenPeriodsOnTheBlockTimelineWithOneBlockOfBudget)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    use_dummy_driver();
    constexpr std::uint32_t K_WARMUP = 2;
    constexpr std::uint32_t K_ACTIVE = 20;
    SushiDSP::Sdl2AudioDevice device(SushiDSP::DeviceFormat{48000.0, 256, 0, 2});
    const SushiDSP::DeviceFormat format = device.format();
    SushiDSP::profile::ProfileSession session;
    ASSERT_TRUE(session
                    .start_window(SushiDSP::profile::names::K_BLOCK_TIMELINE, K_WARMUP, K_ACTIVE)
                    .started());
    std::atomic<std::uint32_t> blocks{0};

    device.start(
        [&](const float* const*, float* const*, std::uint32_t)
        {
            blocks.fetch_add(1);
        });
    const bool ran = wait_until([&] { return blocks.load() >= K_WARMUP + K_ACTIVE + 3U; });
    device.stop();
    const SushiDSP::profile::ProfileFinish finished = session.finish();

    ASSERT_TRUE(ran);
    const SushiDSP::profile::TimelineSummary summary =
        finished.record.summarise(SushiDSP::profile::names::K_BLOCK_TIMELINE);
    EXPECT_EQ(summary.periods, K_ACTIVE);
    EXPECT_NEAR(summary.budget_ns, 1.0e9 * format.block_size / format.sample_rate, 1.0);
    const SushiDSP::profile::ProfileSubject* const gaps =
        finished.record.find("counter:device.arrival_gap");
    ASSERT_NE(gaps, nullptr);
    EXPECT_EQ(gaps->samples.size(), K_ACTIVE);
}
