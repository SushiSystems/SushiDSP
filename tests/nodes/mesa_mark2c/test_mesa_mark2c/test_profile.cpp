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
 * @brief Tests that MesaMark2c writes its region zones and its per-block counters.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/profiled_node.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>

#include <cstdint>

namespace
{
    /// @brief Holds the warm-up periods of the run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the run below.
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief Holds the frames of one control period, K_CONTROL_PERIOD_FRAMES in
    /// mesa_mark2c.cpp; the node renders a block as these, each with its own region zones.
    constexpr std::uint32_t K_CONTROL_PERIOD_FRAMES = 32;

    /// @brief Holds the region zones one block writes, one per control period.
    constexpr std::uint32_t K_ZONES_PER_BLOCK =
        SushiDSP::bench::K_BLOCK_SIZE / K_CONTROL_PERIOD_FRAMES;
} // namespace

TEST(Medium_MesaMark2cProfile, ProcessWritesEachRegionZoneOncePerControlPeriodAndItsCounters)
{
    namespace regions = SushiDSP::profile::names::mesa_mark2c;
    const SushiDSP::profile::ProfiledRun run =
        SushiDSP::bench::profile_node<SushiDSP::MesaMark2c>(K_WARMUP, K_ACTIVE);
    if (!run.start.started())
        GTEST_SKIP() << "this build records nothing";

    EXPECT_EQ(run.record.active_periods(), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(regions::K_INPUT_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_UPSAMPLE_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_LOOP_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_DOWNSAMPLE_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    EXPECT_EQ(run.record.zone_calls(regions::K_CLAMP_ZONE), K_ACTIVE * K_ZONES_PER_BLOCK);
    const SushiDSP::profile::ProfileSubject* const clamped =
        run.record.find("counter:output.clamped");
    ASSERT_NE(clamped, nullptr);
    EXPECT_EQ(clamped->samples.size(), K_ACTIVE);
    const SushiDSP::profile::ProfileSubject* const solves =
        run.record.find("counter:newton.solves");
    ASSERT_NE(solves, nullptr);
    for (const double count : solves->samples)
        EXPECT_GT(count, 0.0);
}
