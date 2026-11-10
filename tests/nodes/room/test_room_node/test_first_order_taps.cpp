/**************************************************************************/
/* test_first_order_taps.cpp                                              */
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
 * @file test_first_order_taps.cpp
 * @brief Tests for RoomNode: that the early taps render where the image source solver placed the
 * first-order paths.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;

    /// @brief Holds the blocks of the render: 2048 frames, past the 914.9 frames of the longest
    /// first-order path and short of the tail's entry at 1749 frames for the paths it reads.
    constexpr std::uint32_t K_RENDER_BLOCKS = 8;

    /// @brief Holds the smallest magnitude a peak must stand at.
    constexpr float K_PEAK_FLOOR = 1e-3f;

    /// @brief Holds the frames either side of a path the peak is searched in.
    constexpr long K_SEARCH_FRAMES = 8;

    /// @brief Bounds how far a peak is from its path's frame: half a frame for the integer index
    /// of the peak, and one frame for the group delay of the tap's fitted filter, which puts the
    /// ceiling image one frame late.
    constexpr double K_PEAK_TOLERANCE_FRAMES = 0.5 + 1.0;

    /// @brief Names one first-order path of the example scene.
    struct PathRow
    {
            /// @brief Names the row.
            const char* name;

            /// @brief Stores the path's length, in metres.
            double metres;
    };

    /// @brief Holds the paths: the cabinet at (1.5, 2.0, 0.5) and the microphone at (4.0, 2.0, 1.2)
    /// in the 6.0 x 4.0 x 2.5 m garage; the two side walls share one path length.
    const PathRow K_PATHS[] = {
        {"Direct", std::sqrt(2.5 * 2.5 + 0.7 * 0.7)},
        {"Floor", std::sqrt(2.5 * 2.5 + 1.7 * 1.7)},
        {"Ceiling", std::sqrt(2.5 * 2.5 + 3.3 * 3.3)},
        {"SideWalls", std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7)},
        {"MinusXWall", std::sqrt(5.5 * 5.5 + 0.7 * 0.7)},
        {"PlusXWall", std::sqrt(6.5 * 6.5 + 0.7 * 0.7)},
    };

    /// @brief Returns the index of the local maximum of |ir| nearest a frame, or -1 when no
    /// local maximum above the floor lies within the window.
    long nearest_peak(const std::vector<float>& ir, double frame, long window, float floor)
    {
        long best = -1;
        double best_distance = 1e9;
        const long low = std::max<long>(1, static_cast<long>(std::floor(frame)) - window);
        const long high = std::min<long>(static_cast<long>(ir.size()) - 2,
                                         static_cast<long>(std::ceil(frame)) + window);
        for (long index = low; index <= high; ++index)
        {
            const float here = std::fabs(ir[static_cast<std::size_t>(index)]);
            const bool peak = here > std::fabs(ir[static_cast<std::size_t>(index - 1)]) &&
                              here >= std::fabs(ir[static_cast<std::size_t>(index + 1)]) &&
                              here > floor;
            const double distance = std::fabs(static_cast<double>(index) - frame);
            if (peak && distance < best_distance)
            {
                best_distance = distance;
                best = index;
            }
        }
        return best;
    }

    /// @brief Returns the microphone's impulse response of the example scene with the 1 cm
    /// cabinet, rendered once. Nothing but taps reaches the first 1749 frames.
    const std::vector<float>& tap_response()
    {
        static const std::vector<float> response = []
        {
            SushiDSP::room::RoomScene scene = microphone_scene();
            scene.cabinet = point_cabinet();
            return render(*prepared_node(scene, K_LISTENER_MICROPHONE),
                          impulse(K_RENDER_BLOCKS * K_BLOCK));
        }();
        return response;
    }

    /// @brief Names a path row for the test names.
    std::string path_name(const ::testing::TestParamInfo<PathRow>& info)
    {
        return info.param.name;
    }
} // namespace

class Medium_RoomNodeFirstOrderTaps : public ::testing::TestWithParam<PathRow>
{
};

TEST_P(Medium_RoomNodeFirstOrderTaps, RoomNodePlacesTheFirstOrderPeakWhereTheSolverSaid)
{
    const double frames = GetParam().metres * K_FRAMES_PER_METRE;
    const long peak = nearest_peak(tap_response(), frames, K_SEARCH_FRAMES, K_PEAK_FLOOR);
    ASSERT_GE(peak, 0) << "no peak near " << frames;
    EXPECT_NEAR(static_cast<double>(peak), frames, K_PEAK_TOLERANCE_FRAMES);
}

INSTANTIATE_TEST_SUITE_P(Paths, Medium_RoomNodeFirstOrderTaps, ::testing::ValuesIn(K_PATHS),
                         path_name);
