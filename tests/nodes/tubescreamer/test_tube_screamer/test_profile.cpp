/**************************************************************************/
/* test_profile.cpp                                                       */
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
 * @file test_profile.cpp
 * @brief Tests that TubeScreamer writes its region zones and flushes its solver counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

#include <cstdint>

namespace
{
    /// @brief Holds the warm-up periods of the run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the run below.
    constexpr std::uint32_t K_ACTIVE = 4;
} // namespace

TEST(Medium_TubeScreamerProfile, ProcessWritesItsRegionsAndFlushesItsSolverCountersEachBlock)
{
    namespace regions = SushiDSP::profile::names::tubescreamer;
    const SushiDSP::profile::ProfiledRun run =
        SushiDSP::bench::profile_node<SushiDSP::TubeScreamer>(K_WARMUP, K_ACTIVE);
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    EXPECT_EQ(run.record.zone_calls(regions::K_INPUT_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_UPSAMPLE_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_LOOP_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_DOWNSAMPLE_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_OUTPUT_ZONE), K_ACTIVE);
    const SushiDSP::profile::ProfileSubject* const solves =
        run.record.find("counter:solver.solves");
    ASSERT_NE(solves, nullptr);
    ASSERT_EQ(solves->samples.size(), K_ACTIVE);
    for (const double count : solves->samples)
    {
        // One clip-stage solve per oversampled sample: 256 frames at 4x.
        EXPECT_DOUBLE_EQ(count, 1024.0);
    }
}
