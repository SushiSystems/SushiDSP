/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of TrackRecorder perform no heap allocation.
 * @author Mustafa Garip
 */

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>

TEST(Medium_TrackRecorder, WritePerformsNoHeapAllocation)
{
    // The trap sees the ring copy and the drop counter. See tests/nodes/capture/test_track_recorder/README.md.
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "sushidsp_rt_safety_recorder";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    SushiDSP::TrackRecorder recorder(2u, 48000.0, 65536);
    ASSERT_TRUE(recorder.arm((root / "rt_safety.wav").string()));

    std::vector<float> left(256, 0.25f);
    std::vector<float> right(256, -0.25f);
    const float* channels[2] = {left.data(), right.data()};

    ASSERT_TRUE(recorder.write(channels, 256u));
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (recorder.frames_written() < 256u && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_GE(recorder.frames_written(), 256u);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 200; ++block)
    {
        recorder.write(channels, 256u);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);

    recorder.stop();
    std::filesystem::remove_all(root);
}
