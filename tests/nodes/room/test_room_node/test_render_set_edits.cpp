/**************************************************************************/
/* test_render_set_edits.cpp                                              */
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
 * @file test_render_set_edits.cpp
 * @brief Tests for the edits RoomNode makes to a built render set: the direct image, the
 * rounded propagation delay and the modal gains a move publishes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/driver_field.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"
#include "support/axis_hrtf.hpp"
#include "support/tolerance.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;
    using SushiDSP::test::axis_hrtf;
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::RoomRenderSet;
    using SushiDSP::room::RoomScene;

    /// @brief Counts the rounded operations between a tap's stored delay and its compensated one:
    /// the subtraction and the cast back to float.
    constexpr std::size_t K_COMPENSATION_OPERATIONS = 2;

    /// @brief Builds the render set of one scene for one listener over the six-axis head.
    std::unique_ptr<RoomRenderSet> build_set(const RoomScene& scene, ListenerKind kind)
    {
        const SushiDSP::room::HrtfSet hrtf = axis_hrtf(K_SAMPLE_RATE);
        return SushiDSP::room::build_render_set(scene, hrtf, K_SAMPLE_RATE, kind);
    }

    /// @brief Returns the ears' set of the microphone scene, built once.
    const RoomRenderSet& ears_set()
    {
        static const std::unique_ptr<RoomRenderSet> set =
            build_set(microphone_scene(), ListenerKind::Ears);
        return *set;
    }

    /// @brief Returns the microphone's set of the microphone scene, built once.
    const RoomRenderSet& microphone_set()
    {
        static const std::unique_ptr<RoomRenderSet> set =
            build_set(microphone_scene(), ListenerKind::Microphone);
        return *set;
    }

    /// @brief Returns the largest distance in metres from the baffle's centre to a point of one of
    /// its cones: the driver offset along both axes and the cone's radius.
    double cone_reach_m(const SushiDSP::room::CabinetGeometry& cabinet)
    {
        return std::sqrt(2.0) * 0.5 * cabinet.driver_spacing_m + cabinet.cone_radius_m;
    }

    /// @brief Returns the worst error ratio of a reflection's delay against the compensated one:
    /// the stored delay less the shift, held at one frame.
    TestSupport::ErrorRatio worst_reflection_ratio(const SushiDSP::room::EarlyTapSet& before,
                                                   const SushiDSP::room::EarlyTapSet& after,
                                                   float shift)
    {
        TestSupport::ErrorRatio worst;
        for (std::uint32_t tap = 1; tap < after.count; ++tap)
        {
            const TestSupport::ErrorRatio ratio = TestSupport::measure_relative_error<float>(
                after.taps[tap].delay_frames, std::max(before.taps[tap].delay_frames - shift, 1.0f),
                K_COMPENSATION_OPERATIONS);
            if (ratio.ratio >= worst.ratio)
            {
                worst = ratio;
            }
        }
        return worst;
    }

    /// @brief Holds what a pair of sets' modal designs share and where they differ.
    struct ModeComparison
    {
            /// @brief Stores whether every mode kept its section.
            bool sections_equal = true;

            /// @brief Stores whether every mode kept its source gain.
            bool source_gains_equal = true;

            /// @brief Stores whether every mode kept its ear gains.
            bool ear_gains_equal = true;

            /// @brief Counts the modes whose source gain changed.
            std::uint32_t source_gains_moved = 0;

            /// @brief Counts the modes whose ear gains changed.
            std::uint32_t ear_gains_moved = 0;
    };

    /// @brief Compares the modal designs of two sets mode by mode.
    ModeComparison compare_modes(const RoomRenderSet& before, const RoomRenderSet& after)
    {
        ModeComparison comparison;
        for (std::uint32_t index = 0; index < before.modal.count; ++index)
        {
            const SushiDSP::room::ModalDesign::Mode& base = before.modal.modes[index];
            const SushiDSP::room::ModalDesign::Mode& other = after.modal.modes[index];
            comparison.sections_equal = comparison.sections_equal && base.section == other.section;
            comparison.source_gains_equal =
                comparison.source_gains_equal && base.source_gain == other.source_gain;
            comparison.ear_gains_equal =
                comparison.ear_gains_equal && base.ear_gain == other.ear_gain;
            comparison.source_gains_moved += base.source_gain != other.source_gain ? 1u : 0u;
            comparison.ear_gains_moved += base.ear_gain != other.ear_gain ? 1u : 0u;
        }
        return comparison;
    }

    /// @brief Counts the modes of a room with a non-zero index along one axis, the modes a move
    /// along that axis changes the gain of: a cosine of index zero is one wherever it is read.
    std::uint32_t modes_along(const RoomScene& scene, double crossover_hz, std::uint32_t axis)
    {
        std::uint32_t count = 0;
        for (const SushiDSP::room::ModalSolver::Mode& mode :
             SushiDSP::room::ModalSolver::enumerate(scene, crossover_hz))
        {
            const std::uint32_t index = axis == 0u ? mode.nx : axis == 1u ? mode.ny : mode.nz;
            count += index != 0u ? 1u : 0u;
        }
        return count;
    }

    /// @brief Returns the wave scene's ears set, built once.
    const RoomRenderSet& wave_before()
    {
        static const std::unique_ptr<RoomRenderSet> set =
            build_set(wave_scene(), ListenerKind::Ears);
        return *set;
    }

    /// @brief Returns the wave scene's ears set with the cabinet 0.2 m along x, built once.
    const RoomRenderSet& wave_cabinet_moved()
    {
        static const std::unique_ptr<RoomRenderSet> set = []
        {
            RoomScene scene = wave_scene();
            scene.source_position.x += 0.2;
            return build_set(scene, ListenerKind::Ears);
        }();
        return *set;
    }

    /// @brief Returns the wave scene's ears set with the listener 0.3 m along -y, built once.
    const RoomRenderSet& wave_listener_moved()
    {
        static const std::unique_ptr<RoomRenderSet> set = []
        {
            RoomScene scene = wave_scene();
            scene.listener_position.y -= 0.3;
            return build_set(scene, ListenerKind::Ears);
        }();
        return *set;
    }

    /// @brief Returns the wave scene's ears set with the room 0.1 m wider, built once.
    const RoomRenderSet& wave_widened()
    {
        static const std::unique_ptr<RoomRenderSet> set = []
        {
            RoomScene scene = wave_scene();
            scene.dimensions.x += 0.1;
            return build_set(scene, ListenerKind::Ears);
        }();
        return *set;
    }

    /// @brief Builds the sets the direct image and the delay compensation read, once per suite, so no
    /// case is charged for them.
    class EarsAndMicrophoneSets : public ::testing::Test
    {
        public:
            static void SetUpTestSuite()
            {
                ears_set();
                microphone_set();
            }
    };

    /// @brief Builds the wave scene's four sets, once per suite, so no case is charged for them.
    class WaveSets : public ::testing::Test
    {
        public:
            static void SetUpTestSuite()
            {
                wave_before();
                wave_cabinet_moved();
                wave_listener_moved();
                wave_widened();
            }
    };
} // namespace

class Small_RoomNodeDirectImage : public EarsAndMicrophoneSets
{
};

class Small_RoomNodeDirectDelayCompensation : public EarsAndMicrophoneSets
{
};

class Small_RoomNodeModalPublication : public WaveSets
{
};

TEST_F(Small_RoomNodeDirectImage, RoomNodeTakesTheFirstTapOfTheEarsBankAsTheDirectImage)
{
    const RoomRenderSet& ears = ears_set();
    SushiDSP::room::EarlyTapSet reflections{};
    SushiDSP::room::EarlyTap direct_tap{};
    ASSERT_GT(ears.early.count, 1u);
    EXPECT_TRUE(SushiDSP::room::RoomNode::take_direct_image(ears, reflections, direct_tap));
    EXPECT_EQ(reflections.count, ears.early.count - 1u);
    EXPECT_EQ(direct_tap.delay_frames, ears.early.taps[0].delay_frames);
    EXPECT_EQ(reflections.taps[0].delay_frames, ears.early.taps[1].delay_frames);
    EXPECT_EQ(reflections.crossover_time_s, ears.early.crossover_time_s);
}

TEST_F(Small_RoomNodeDirectImage, RoomNodeLeavesOnlyTapsAfterTheDirectImageInTheEarsBank)
{
    const RoomRenderSet& ears = ears_set();
    SushiDSP::room::EarlyTapSet reflections{};
    SushiDSP::room::EarlyTap direct_tap{};
    ASSERT_TRUE(SushiDSP::room::RoomNode::take_direct_image(ears, reflections, direct_tap));
    const float direct = ears.early.taps[0].delay_frames;
    EXPECT_TRUE(std::all_of(reflections.taps.begin(), reflections.taps.begin() + reflections.count,
                            [direct](const SushiDSP::room::EarlyTap& tap)
                            { return tap.delay_frames > direct; }));
}

TEST_F(Small_RoomNodeDirectImage, RoomNodeLeavesAMicrophoneBankWhole)
{
    const RoomRenderSet& microphone = microphone_set();
    SushiDSP::room::EarlyTapSet reflections{};
    SushiDSP::room::EarlyTap direct_tap{};
    constexpr float K_UNTOUCHED = -1.0f;
    direct_tap.delay_frames = K_UNTOUCHED;
    EXPECT_FALSE(SushiDSP::room::RoomNode::take_direct_image(microphone, reflections, direct_tap));
    EXPECT_EQ(reflections.count, microphone.early.count);
    EXPECT_EQ(direct_tap.delay_frames, K_UNTOUCHED);
}

TEST_F(Small_RoomNodeDirectImage, RoomNodeOpensAMicrophoneBankWithTheCabinetsElements)
{
    // The nearest element stands at most one cone's reach nearer than the baffle's centre.
    const RoomRenderSet& microphone = microphone_set();
    SushiDSP::room::EarlyTapSet reflections{};
    SushiDSP::room::EarlyTap direct_tap{};
    SushiDSP::room::RoomNode::take_direct_image(microphone, reflections, direct_tap);
    const SushiDSP::room::DriverField field(SushiDSP::room::K_CABINET4X12);
    const double centre_frames = static_cast<double>(ears_set().early.taps[0].delay_frames);
    EXPECT_EQ(reflections.direct_count, field.element_count());
    EXPECT_LT(static_cast<double>(reflections.taps[0].delay_frames), centre_frames);
    EXPECT_GT(static_cast<double>(reflections.taps[0].delay_frames),
              centre_frames - K_FRAMES_PER_METRE * cone_reach_m(SushiDSP::room::K_CABINET4X12));
}

TEST_F(Small_RoomNodeDirectDelayCompensation, RoomNodeRoundsTheEarsDirectDelayToWholeFrames)
{
    const RoomRenderSet& ears = ears_set();
    EXPECT_EQ(ears.direct_delay_frames, std::round(ears.early.taps[0].delay_frames));
}

TEST_F(Small_RoomNodeDirectDelayCompensation, RoomNodeMovesTheEarsDirectTapByTheRoundedDelay)
{
    const std::unique_ptr<RoomRenderSet> ears = std::make_unique<RoomRenderSet>(ears_set());
    const float shift = ears->direct_delay_frames;
    SushiDSP::room::RoomNode::compensate_direct_delay(*ears);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        ears->early.taps[0].delay_frames, ears_set().early.taps[0].delay_frames - shift,
        K_COMPENSATION_OPERATIONS)));
}

TEST_F(Small_RoomNodeDirectDelayCompensation, RoomNodeKeepsEachReflectionsSpacingToTheDirectTap)
{
    const std::unique_ptr<RoomRenderSet> ears = std::make_unique<RoomRenderSet>(ears_set());
    const float shift = ears->direct_delay_frames;
    SushiDSP::room::RoomNode::compensate_direct_delay(*ears);
    EXPECT_TRUE(TestSupport::is_within(
        worst_reflection_ratio(ears_set().early, ears->early, shift)));
}

TEST_F(Small_RoomNodeDirectDelayCompensation, RoomNodeMovesTheTailInputWithTheEarsDirectTap)
{
    const std::unique_ptr<RoomRenderSet> ears = std::make_unique<RoomRenderSet>(ears_set());
    const float shift = ears->direct_delay_frames;
    SushiDSP::room::RoomNode::compensate_direct_delay(*ears);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        ears->late.delay_to_crossover_frames,
        std::max(ears_set().late.delay_to_crossover_frames - shift, 0.0f),
        K_COMPENSATION_OPERATIONS)));
}

TEST_F(Small_RoomNodeDirectDelayCompensation, RoomNodeLeavesAMicrophoneSetAsItWas)
{
    const std::unique_ptr<RoomRenderSet> microphone =
        std::make_unique<RoomRenderSet>(microphone_set());
    SushiDSP::room::RoomNode::compensate_direct_delay(*microphone);
    EXPECT_EQ(microphone->early.taps[0].delay_frames, microphone_set().early.taps[0].delay_frames);
    EXPECT_EQ(microphone->late.delay_to_crossover_frames,
              microphone_set().late.delay_to_crossover_frames);
}

TEST_F(Small_RoomNodeModalPublication, RoomNodeKeepsTheResonatorsWhenTheCabinetMoves)
{
    EXPECT_TRUE(SushiDSP::room::RoomNode::same_resonators(wave_before().modal,
                                                          wave_cabinet_moved().modal));
    EXPECT_EQ(wave_before().modal.level_scale, wave_cabinet_moved().modal.level_scale);
}

TEST_F(Small_RoomNodeModalPublication, RoomNodeKeepsTheResonatorsWhenTheListenerMoves)
{
    EXPECT_TRUE(SushiDSP::room::RoomNode::same_resonators(wave_before().modal,
                                                          wave_listener_moved().modal));
}

TEST_F(Small_RoomNodeModalPublication, RoomNodeRebuildsTheResonatorsWhenTheRoomChanges)
{
    EXPECT_FALSE(
        SushiDSP::room::RoomNode::same_resonators(wave_before().modal, wave_widened().modal));
}

TEST_F(Small_RoomNodeModalPublication, RoomNodeChangesTheSourceGainsOfTheModesAlongTheCabinetsMove)
{
    // Every mode with a non-zero x index feels the cabinet's move along x.
    const RoomRenderSet& before = wave_before();
    ASSERT_GT(before.modal.count, 0u);
    const ModeComparison comparison = compare_modes(before, wave_cabinet_moved());
    EXPECT_TRUE(comparison.sections_equal);
    EXPECT_TRUE(comparison.ear_gains_equal);
    EXPECT_EQ(comparison.source_gains_moved, modes_along(wave_scene(), before.crossover_hz, 0u));
}

TEST_F(Small_RoomNodeModalPublication, RoomNodeChangesTheEarGainsOfTheModesAlongTheListenersMove)
{
    // Every mode with a non-zero y index feels the listener's move along y.
    const RoomRenderSet& before = wave_before();
    ASSERT_GT(before.modal.count, 0u);
    const ModeComparison comparison = compare_modes(before, wave_listener_moved());
    EXPECT_TRUE(comparison.sections_equal);
    EXPECT_TRUE(comparison.source_gains_equal);
    EXPECT_EQ(comparison.ear_gains_moved, modes_along(wave_scene(), before.crossover_hz, 1u));
}
