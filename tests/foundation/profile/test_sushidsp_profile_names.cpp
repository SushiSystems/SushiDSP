/**************************************************************************/
/* test_sushidsp_profile_names.cpp                                        */
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
 * @file test_sushidsp_profile_names.cpp
 * @brief Tests that the names a record and a checks file agree on are distinct.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <cstddef>
#include <vector>
#include <cstdint>
#include <string>

namespace
{
    /// @brief Holds the warm-up periods of every run below.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of every run below.
    constexpr std::uint32_t K_ACTIVE = 5;

    /// @brief Holds the timeline every run below follows.
    constexpr const char* K_TIMELINE = SushiDSP::profile::names::K_BENCH_TIMELINE;
} // namespace

TEST(Small_SushidspProfileNames, EveryTimelineZoneAndCounterNameIsDistinctAndNotEmpty)
{
    namespace names = SushiDSP::profile::names;
    const std::vector<std::string> all{
        names::K_BLOCK_TIMELINE,       names::K_BENCH_TIMELINE,   names::K_PROCESS_ZONE,
        names::K_GRAPH_PROCESS_ZONE,   names::K_NODE_ZONE,        names::K_GATHER_ZONE,
        names::K_SESSION_LANE_ZONE,    names::K_SESSION_RIG_ZONE, names::K_SESSION_MIX_ZONE,
        names::K_SESSION_RECORDER_ZONE, names::K_SESSION_METRONOME_ZONE,
        names::K_ROOM_TAIL_ZONE,       names::K_ROOM_MODAL_ZONE,  names::K_ROOM_PAIR_ZONE,
        names::K_ROOM_EARS_ZONE,       names::K_ROOM_MICROPHONES_ZONE,
        names::K_ARRIVAL_GAP_COUNTER,  names::K_OVERRUN_COUNTER,  names::K_NON_CONVERGED_COUNTER,
        names::K_NAN_FALLBACK_COUNTER, names::K_SOLVES_COUNTER,   names::K_WORST_ITERATIONS_COUNTER,
        names::K_NEWTON_SOLVES_COUNTER, names::K_NEWTON_CAPS_COUNTER,
        names::K_NEWTON_NON_FINITE_COUNTER, names::K_NEWTON_WORST_ITERATIONS_COUNTER,
        names::K_CLAMPED_COUNTER,
        names::jcm800::K_LOOP_ZONE,    names::peavey5150::K_LOOP_ZONE,
        names::mesa_mark2c::K_LOOP_ZONE, names::diezel_vh4::K_LOOP_ZONE,
        names::tubescreamer::K_LOOP_ZONE};

    for (std::size_t first = 0; first < all.size(); ++first)
    {
        EXPECT_FALSE(all[first].empty());
        for (std::size_t second = first + 1; second < all.size(); ++second)
            EXPECT_NE(all[first], all[second]) << first << " and " << second;
    }
}
