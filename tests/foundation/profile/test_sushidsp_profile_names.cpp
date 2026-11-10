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
 * @file test_sushidsp_profile_names.cpp
 * @brief Tests that the names a record and a checks file agree on are distinct.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace
{
    namespace names = SushiDSP::profile::names;

    /// @brief Expects every name of @p all to be non-empty and to differ from each name after it.
    void expect_distinct_and_not_empty(const std::vector<std::string_view>& all)
    {
        for (std::size_t first = 0; first < all.size(); ++first)
        {
            EXPECT_FALSE(all[first].empty()) << first;
            for (std::size_t second = first + 1; second < all.size(); ++second)
                EXPECT_NE(all[first], all[second]) << first << " and " << second;
        }
    }

    /// @brief Returns the timeline names.
    std::vector<std::string_view> timeline_names()
    {
        return {names::K_BLOCK_TIMELINE, names::K_BENCH_TIMELINE};
    }

    /// @brief Returns the zone names of the graph, the session and the room.
    std::vector<std::string_view> engine_zone_names()
    {
        return {names::K_PROCESS_ZONE,           names::K_GRAPH_PROCESS_ZONE,
                names::K_NODE_ZONE,              names::K_GATHER_ZONE,
                names::K_SESSION_LANE_ZONE,      names::K_SESSION_RIG_ZONE,
                names::K_SESSION_MIX_ZONE,       names::K_SESSION_RECORDER_ZONE,
                names::K_SESSION_METRONOME_ZONE, names::K_ROOM_TAIL_ZONE,
                names::K_ROOM_MODAL_ZONE,        names::K_ROOM_PAIR_ZONE,
                names::K_ROOM_EARS_ZONE,         names::K_ROOM_MICROPHONES_ZONE};
    }

    /// @brief Returns the zone names of the five node regions: input, upsample, loop,
    /// downsample, post or output, and clamp.
    std::vector<std::string_view> node_zone_names()
    {
        return {names::jcm800::K_INPUT_ZONE,         names::jcm800::K_UPSAMPLE_ZONE,
                names::jcm800::K_LOOP_ZONE,          names::jcm800::K_DOWNSAMPLE_ZONE,
                names::jcm800::K_POST_ZONE,          names::jcm800::K_CLAMP_ZONE,
                names::peavey5150::K_INPUT_ZONE,     names::peavey5150::K_UPSAMPLE_ZONE,
                names::peavey5150::K_LOOP_ZONE,      names::peavey5150::K_DOWNSAMPLE_ZONE,
                names::peavey5150::K_POST_ZONE,      names::peavey5150::K_CLAMP_ZONE,
                names::mesa_mark2c::K_INPUT_ZONE,    names::mesa_mark2c::K_UPSAMPLE_ZONE,
                names::mesa_mark2c::K_LOOP_ZONE,     names::mesa_mark2c::K_DOWNSAMPLE_ZONE,
                names::mesa_mark2c::K_POST_ZONE,     names::mesa_mark2c::K_CLAMP_ZONE,
                names::diezel_vh4::K_INPUT_ZONE,     names::diezel_vh4::K_UPSAMPLE_ZONE,
                names::diezel_vh4::K_LOOP_ZONE,      names::diezel_vh4::K_DOWNSAMPLE_ZONE,
                names::diezel_vh4::K_POST_ZONE,      names::diezel_vh4::K_CLAMP_ZONE,
                names::tubescreamer::K_INPUT_ZONE,   names::tubescreamer::K_UPSAMPLE_ZONE,
                names::tubescreamer::K_LOOP_ZONE,    names::tubescreamer::K_DOWNSAMPLE_ZONE,
                names::tubescreamer::K_OUTPUT_ZONE};
    }

    /// @brief Returns the counter names.
    std::vector<std::string_view> counter_names()
    {
        return {names::K_ARRIVAL_GAP_COUNTER,
                names::K_OVERRUN_COUNTER,
                names::K_NON_CONVERGED_COUNTER,
                names::K_NAN_FALLBACK_COUNTER,
                names::K_SOLVES_COUNTER,
                names::K_WORST_ITERATIONS_COUNTER,
                names::K_NEWTON_SOLVES_COUNTER,
                names::K_NEWTON_CAPS_COUNTER,
                names::K_NEWTON_NON_FINITE_COUNTER,
                names::K_NEWTON_WORST_ITERATIONS_COUNTER,
                names::K_CLAMPED_COUNTER};
    }

    /// @brief Returns every timeline, zone and counter name, which a record holds side by side.
    std::vector<std::string_view> all_recorded_names()
    {
        std::vector<std::string_view> all = timeline_names();
        for (const auto& group : {engine_zone_names(), node_zone_names(), counter_names()})
            all.insert(all.end(), group.begin(), group.end());
        return all;
    }
} // namespace

TEST(Small_SushidspProfileNames, EveryTimelineZoneAndCounterNameIsDistinctAndNotEmpty)
{
    expect_distinct_and_not_empty(all_recorded_names());
}
