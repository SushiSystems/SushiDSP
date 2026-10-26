/**************************************************************************/
/* test_session_renderer.cpp                                              */
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
 * @file test_session_renderer.cpp
 * @brief Tests the block renderer's counters, its profiler zones and its one lane per track.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "session.hpp"
#include "session_renderer.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds the block every case renders in.
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds what every session of the cases is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, K_BLOCK, 2};

    /// @brief Holds the warm-up periods of the profiled cases.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the profiled cases.
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief Holds two output channels and the silent live input a render reads.
    struct Buffers
    {
            std::vector<float> left;  ///< Holds the left output.
            std::vector<float> right; ///< Holds the right output.
            std::vector<float> live;  ///< Holds the live input, both channels.

            /// @brief Sizes every buffer to @p frames.
            explicit Buffers(std::uint32_t frames)
                : left(frames, 0.5f), right(frames, 0.5f), live(frames, 0.0f)
            {

            }
    };
} // namespace

TEST(Small_SessionRenderer, ABlockLongerThanThePreparedSizeIsCutAndCounted)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(2 * K_BLOCK);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    renderer.render(session, nullptr, outputs, 2 * K_BLOCK);

    EXPECT_EQ(renderer.counters().cut_block.value(), 1U);
    EXPECT_EQ(renderer.counters().cut_block.peak(), 2U * K_BLOCK);
}

TEST(Small_SessionRenderer, ABlockInsideThePreparedSizeCountsNothing)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(K_BLOCK);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    renderer.render(session, nullptr, outputs, K_BLOCK);

    EXPECT_EQ(renderer.counters().cut_block.value(), 0U);
}

TEST(Small_SessionRenderer, TheCutBlockKeepsTheSamplesOfItsRenderedPrefixCleared)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(2 * K_BLOCK);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    renderer.render(session, nullptr, outputs, 2 * K_BLOCK);

    for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
        EXPECT_FLOAT_EQ(buffers.left[frame], 0.0f) << frame;
}

TEST(Medium_SessionRenderer, RenderingEightRollingTracksWritesEightLanesAndTheProcessZone)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    Session session(K_INFO);
    std::string error;
    while (session.track_count() < Session::K_MAX_TRACKS)
        ASSERT_TRUE(session.add_track(error)) << error;
    session.timeline().transport().play();
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(K_BLOCK);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        SushiDSP::profile::names::K_BENCH_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
        [&]
        {
            renderer.render(session, nullptr, outputs, K_BLOCK);
        });

    ASSERT_TRUE(run.start.started());
    const std::vector<std::string> lanes = run.record.lanes();
    for (std::size_t track = 0; track < Session::K_MAX_TRACKS; ++track)
    {
        const std::string name = "track." + std::to_string(track);
        EXPECT_NE(std::find(lanes.begin(), lanes.end(), name), lanes.end()) << name;
    }
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_PROCESS_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_SESSION_MIX_ZONE),
              K_ACTIVE * Session::K_MAX_TRACKS);
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_SESSION_METRONOME_ZONE),
              K_ACTIVE);
}

TEST(Medium_SessionRenderer, AProfiledRenderGivesTheSamplesAnUnprofiledOneGives)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    std::vector<float> live(K_BLOCK, 0.25f);
    const float* inputs[2] = {live.data(), live.data()};
    Buffers plain(K_BLOCK);
    float* plain_outputs[2] = {plain.left.data(), plain.right.data()};
    renderer.render(session, inputs, plain_outputs, K_BLOCK);

    Buffers profiled(K_BLOCK);
    float* profiled_outputs[2] = {profiled.left.data(), profiled.right.data()};
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        SushiDSP::profile::names::K_BENCH_TIMELINE, 0U, 1U, 0U,
        [&]
        {
            renderer.render(session, inputs, profiled_outputs, K_BLOCK);
        });
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    EXPECT_EQ(plain.left, profiled.left);
    EXPECT_EQ(plain.right, profiled.right);
}
