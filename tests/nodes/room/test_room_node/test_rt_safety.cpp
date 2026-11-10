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
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of RoomNode perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_profile_sink.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room_scenes/room_ports.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    using SushiDSP::room::RoomNode;
    using SushiDSP::room::RoomScene;

    /// @brief Names the block size every rig below runs at.
    constexpr std::uint32_t K_BLOCK_FRAMES = 256;

    /// @brief Names the sample rate every rig below runs at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Returns the garage with an omni microphone in front of the cabinet.
    RoomScene room_trap_scene(double microphone_x)
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {1.5, 2.0, 0.5};
        scene.microphone_position = {microphone_x, 2.0, 1.2};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Fills a block with a 220 Hz tone at 0.3 of full scale.
    void fill_room_tone(std::vector<float>& block)
    {
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            block[i] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f *
                                       static_cast<float>(i) / 48000.0f);
        }
    }

    /// @brief Checks that every sample of a block is a finite number.
    void expect_finite(const std::vector<float>& block)
    {
        for (const float sample : block)
        {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }

    /// @brief Holds one tone input, a stereo output and the context that reads them.
    struct MonoRig
    {
            MonoRig() : input(K_BLOCK_FRAMES), left(K_BLOCK_FRAMES, 0.0f), right(K_BLOCK_FRAMES, 0.0f)
            {
                fill_room_tone(input);
            }

            MonoRig(const MonoRig&) = delete;
            MonoRig& operator=(const MonoRig&) = delete;

            std::vector<float> input;
            std::vector<float> left;
            std::vector<float> right;
            std::array<const float*, 1> input_channels{input.data()};
            std::array<float*, 2> output_channels{left.data(), right.data()};
            SushiDSP::ProcessContext context{output_channels.data(), input_channels.data(), K_BLOCK_FRAMES, 0.0};
    };

    /// @brief Prepares a node and publishes the trap scene; the test asserts the publish.
    void prepare_with_trap_scene(RoomNode& node)
    {
        node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK_FRAMES, 1});
        std::string reason;
        ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;
    }

    /// @brief Processes blocks, writing a parameter to 0 or 1 alternately from the audio thread
    /// before each block, starting at 0.
    void process_alternating(RoomNode& node, SushiDSP::ProcessContext& context, std::uint16_t parameter,
                             int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            node.set_parameter_for_audio_thread(parameter, block % 2 == 0 ? 0.0f : 1.0f);
            node.process(context);
        }
    }

    /// @brief Processes blocks.
    void run_blocks(RoomNode& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            node.process(context);
        }
    }

    /// @brief Publishes the trap scene as an arrangement with one microphone, moved and widened as
    /// the row says.
    void assert_scene_published(RoomNode& node, double microphone_x, double listener_dy, double width_m)
    {
        RoomScene scene = room_trap_scene(microphone_x);
        scene.listener_position.y = 2.0 + listener_dy;
        scene.dimensions.x = width_m;
        SushiDSP::Room::Arrangement arrangement = SushiDSP::Room::Arrangement::from_scene(scene);
        SushiDSP::Room::MicrophonePlacement microphone;
        microphone.id = 1;
        microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
        microphone.pattern = scene.microphone_pattern;
        std::string reason;
        ASSERT_TRUE(arrangement.add_microphone(microphone, reason)) << reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;
    }

    /// @brief Publishes the garage spread to a number of cabinets and microphones, one of them
    /// moved when asked.
    void assert_spread_published(RoomNode& node, std::uint32_t sources, std::uint32_t microphones, bool moved)
    {
        SushiDSP::Room::Arrangement arrangement =
            SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), sources, microphones);
        if (moved)
        {
            SushiDSP::Room::MicrophonePlacement shifted = arrangement.microphones().front();
            shifted.pose.position.x += 0.2;
            ASSERT_TRUE(arrangement.update_microphone(shifted));
        }
        std::string reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;
    }

    /// @brief Holds the state a publish moves a node to, after the base state is playing.
    struct PublishState
    {
            const char* name;
            double microphone_x;
            double listener_dy;
            double width_m;
    };

    /// @brief Holds the base state, microphone at 3.0 m, listener on the cabinet's line, 6.0 m wide.
    constexpr PublishState K_BASE_STATE{"Base", 3.0, 0.0, 6.0};

    /// @brief Holds the states the audio thread adopts and crossfades to.
    const std::array<PublishState, 4> K_PUBLISH_STATES = {{{"MicrophoneMoves", 3.25, 0.0, 6.0},
                                                           {"ListenerMoves", 3.0, 0.1, 6.0},
                                                           {"RoomWidens", 3.0, 0.0, 6.2},
                                                           {"EverythingChanges", 3.75, 0.1, 6.2}}};

    /// @brief Names a publish state case.
    std::string publish_state_name(const ::testing::TestParamInfo<PublishState>& info)
    {
        return info.param.name;
    }

    /// @brief Holds a shape change between two spreads of cabinets and microphones.
    struct SpreadChange
    {
            const char* name;
            std::uint32_t from_sources;
            std::uint32_t from_microphones;
            std::uint32_t to_sources;
            std::uint32_t to_microphones;
            bool moved;
    };

    /// @brief Holds the changes between one cabinet with two microphones and the limits, three
    /// cabinets with eight, and one microphone moving at the limits.
    const std::array<SpreadChange, 3> K_SPREAD_CHANGES = {{{"GrowsToTheLimits", 1, 2, 3, 8, false},
                                                           {"ShrinksFromTheLimits", 3, 8, 1, 2, false},
                                                           {"MovesAMicrophoneAtTheLimits", 3, 8, 3, 8, true}}};

    /// @brief Names a spread change case.
    std::string spread_change_name(const ::testing::TestParamInfo<SpreadChange>& info)
    {
        return info.param.name;
    }

    /// @brief Leaves the node as it is.
    void change_nothing(RoomNode&)
    {
    }

    /// @brief Switches the direct path's delay on from the control thread.
    void switch_direct_delay_on(RoomNode& node)
    {
        node.set_parameter_for_control_thread("direct_delay", 1.0f);
    }

    /// @brief Reinstalls the fallback HRTF and checks that the node took it.
    void expect_fallback_hrtf_installed(RoomNode& node)
    {
        EXPECT_EQ(node.set_hrtf(SushiDSP::room::HrtfSet{}), SushiDSP::room::HrtfStatus::Installed);
    }

    /// @brief Holds a control-thread change made before the binaural blocks.
    struct BinauralChange
    {
            const char* name;
            void (*apply)(RoomNode&);
    };

    /// @brief Holds the changes the binaural path adopts: none, the direct delay and the fallback.
    const std::array<BinauralChange, 3> K_BINAURAL_CHANGES = {{{"Nothing", change_nothing},
                                                               {"DirectDelaySwitches", switch_direct_delay_on},
                                                               {"FallbackHrtfReinstalled",
                                                                expect_fallback_hrtf_installed}}};

    /// @brief Names a binaural change case.
    std::string binaural_change_name(const ::testing::TestParamInfo<BinauralChange>& info)
    {
        return info.param.name;
    }
} // namespace

class Medium_RoomNodePublish : public ::testing::TestWithParam<PublishState>
{
};

class Medium_RoomNodeSpread : public ::testing::TestWithParam<SpreadChange>
{
};

class Medium_RoomNodeBinaural : public ::testing::TestWithParam<BinauralChange>
{
};

TEST(Medium_RoomNode, ProcessPerformsNoHeapAllocation)
{
    // The first trapped block adopts the published set. See tests/nodes/room/test_room_node/README.md.
    RoomNode node;
    ASSERT_NO_FATAL_FAILURE(prepare_with_trap_scene(node));
    MonoRig rig;

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_alternating(node, rig.context, std::uint16_t{0}, 16);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    expect_finite(rig.left);
    expect_finite(rig.right);
}

TEST_P(Medium_RoomNodePublish, ProcessPerformsNoHeapAllocationAcrossAPublish)
{
    // Only the audio thread's half of a publish is trapped. See tests/nodes/room/test_room_node/README.md.
    RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK_FRAMES, 1});
    std::vector<float> input(K_BLOCK_FRAMES);
    fill_room_tone(input);
    const std::array<const float*, 2> input_channels = {input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(K_BLOCK_FRAMES);
    SushiDSP::ProcessContext context = ports.context(input_channels.data(), K_BLOCK_FRAMES);
    ASSERT_NO_FATAL_FAILURE(assert_scene_published(node, K_BASE_STATE.microphone_x, K_BASE_STATE.listener_dy,
                                                   K_BASE_STATE.width_m));
    run_blocks(node, context, 3);
    node.reclaim_rooms();
    ASSERT_NO_FATAL_FAILURE(assert_scene_published(node, GetParam().microphone_x, GetParam().listener_dy,
                                                   GetParam().width_m));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    run_blocks(node, context, 3);
    trap.disarm();
    node.reclaim_rooms();

    EXPECT_EQ(trap.count(), 0);
    expect_finite(ports.listener(0));
    expect_finite(ports.microphone(0));
}

INSTANTIATE_TEST_SUITE_P(States, Medium_RoomNodePublish, ::testing::ValuesIn(K_PUBLISH_STATES),
                         publish_state_name);

TEST_P(Medium_RoomNodeSpread, ProcessPerformsNoHeapAllocationAtTheLimits)
{
    // Up to three cabinets and eight microphones, with placements coming and going between blocks.
    RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, K_BLOCK_FRAMES, 1});
    std::vector<float> input(K_BLOCK_FRAMES);
    fill_room_tone(input);
    const std::array<const float*, 6> input_channels = {input.data(), input.data(), input.data(),
                                                        input.data(), input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(K_BLOCK_FRAMES);
    SushiDSP::ProcessContext context = ports.context(input_channels.data(), K_BLOCK_FRAMES);
    ASSERT_NO_FATAL_FAILURE(assert_spread_published(node, GetParam().from_sources, GetParam().from_microphones, false));
    run_blocks(node, context, 4);
    node.reclaim_rooms();
    ASSERT_NO_FATAL_FAILURE(
        assert_spread_published(node, GetParam().to_sources, GetParam().to_microphones, GetParam().moved));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    run_blocks(node, context, 4);
    trap.disarm();
    node.reclaim_rooms();

    EXPECT_EQ(trap.count(), 0);
    expect_finite(ports.listener(0));
    expect_finite(ports.listener(1));
    expect_finite(ports.microphone(0));
}

INSTANTIATE_TEST_SUITE_P(Changes, Medium_RoomNodeSpread, ::testing::ValuesIn(K_SPREAD_CHANGES), spread_change_name);

TEST_P(Medium_RoomNodeBinaural, BinauralProcessPerformsNoHeapAllocation)
{
    // The ears render every binaural path inside the trap. See tests/nodes/room/test_room_node/README.md.
    RoomNode node;
    ASSERT_NO_FATAL_FAILURE(prepare_with_trap_scene(node));
    MonoRig rig;
    GetParam().apply(node);

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_alternating(node, rig.context, std::uint16_t{3}, 4);
    trap.disarm();
    node.reclaim_rooms();

    EXPECT_EQ(trap.count(), 0);
    expect_finite(rig.left);
    expect_finite(rig.right);
}

INSTANTIATE_TEST_SUITE_P(Changes, Medium_RoomNodeBinaural, ::testing::ValuesIn(K_BINAURAL_CHANGES),
                         binaural_change_name);

TEST(Small_RoomNodeCounters, AnUnpreparedNodeCountsABlockWithNoRenderSet)
{
    SushiDSP::room::RoomNode::Counters counts;
    SushiDSP::room::RoomNode node(counts);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};

    node.process(context);

    EXPECT_EQ(counts.no_render_set.value(), 1U);
}

TEST(Medium_RoomNodeCounters, APreparedNodeWithASetCountsNothing)
{
    SushiDSP::room::RoomNode::Counters counts;
    SushiDSP::room::RoomNode node(counts);
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    node.process(context);

    EXPECT_EQ(counts.no_render_set.value(), 0U);
}

TEST(Medium_RoomNodeProfile, ProcessReportsTheRegionZonesToItsSink)
{
    constexpr std::uint32_t K_BLOCKS = 4;
    SushiDSP::room::RoomNode::Counters counts;
    SushiDSP::room::InMemoryRoomProfileSink sink;
    RoomNode node(counts, sink);
    ASSERT_NO_FATAL_FAILURE(prepare_with_trap_scene(node));
    MonoRig rig;

    run_blocks(node, rig.context, K_BLOCKS);

    EXPECT_EQ(sink.entered(SushiDSP::profile::names::K_ROOM_EARS_ZONE), K_BLOCKS);
    EXPECT_EQ(sink.entered(SushiDSP::profile::names::K_ROOM_MICROPHONES_ZONE), K_BLOCKS);
    EXPECT_GE(sink.entered(SushiDSP::profile::names::K_ROOM_TAIL_ZONE), K_BLOCKS);
    EXPECT_GE(sink.entered(SushiDSP::profile::names::K_ROOM_MODAL_ZONE), K_BLOCKS);
    EXPECT_GE(sink.entered(SushiDSP::profile::names::K_ROOM_PAIR_ZONE), K_BLOCKS);
}

TEST(Medium_RoomNodeProfile, ProcessLeavesEveryZoneItEnters)
{
    constexpr std::uint32_t K_BLOCKS = 4;
    SushiDSP::room::RoomNode::Counters counts;
    SushiDSP::room::InMemoryRoomProfileSink sink;
    RoomNode node(counts, sink);
    ASSERT_NO_FATAL_FAILURE(prepare_with_trap_scene(node));
    MonoRig rig;

    run_blocks(node, rig.context, K_BLOCKS);

    EXPECT_EQ(sink.left(SushiDSP::profile::names::K_ROOM_EARS_ZONE),
              sink.entered(SushiDSP::profile::names::K_ROOM_EARS_ZONE));
    EXPECT_EQ(sink.left(SushiDSP::profile::names::K_ROOM_TAIL_ZONE),
              sink.entered(SushiDSP::profile::names::K_ROOM_TAIL_ZONE));
}
