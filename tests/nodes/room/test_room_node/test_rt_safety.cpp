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
 * @brief Tests that the audio-thread paths of RoomNode perform no heap allocation.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES precedes every include so that MSVC's <cmath> declares M_PI.
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
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
    /// @brief Returns the garage with an omni microphone 2.5 m in front of the cabinet.
    SushiDSP::room::RoomScene room_trap_scene(double microphone_x)
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
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
} // namespace

TEST(Small_RoomNode, ProcessPerformsNoHeapAllocation)
{
    // The first trapped block adopts the published set. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    for (int block = 0; block < 16; ++block)
    {
        node.set_parameter_for_audio_thread(std::uint16_t{0}, block % 2 == 0 ? 0.0f : 1.0f);
        node.process(context);
    }
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}

TEST(Small_RoomNode, ProcessPerformsNoHeapAllocationAcrossAPublish)
{
    // Only the audio thread's half of a publish is trapped. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256);
    fill_room_tone(input);
    const float* input_channels[2] = {input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(256);
    SushiDSP::ProcessContext context = ports.context(input_channels, 256);

    SushiDSP::test::AllocationTrap trap;
    for (int publish = 0; publish < 4; ++publish)
    {
        SushiDSP::room::RoomScene scene = room_trap_scene(3.0 + 0.25 * publish);
        scene.listener_position.y = 2.0 + 0.1 * (publish % 2);
        scene.dimensions.x = publish < 2 ? 6.0 : 6.2;
        SushiDSP::Room::Arrangement arrangement = SushiDSP::Room::Arrangement::from_scene(scene);
        SushiDSP::Room::MicrophonePlacement microphone;
        microphone.id = 1;
        microphone.pose = {scene.microphone_position, scene.microphone_yaw_rad};
        microphone.pattern = scene.microphone_pattern;
        std::string reason;
        ASSERT_TRUE(arrangement.add_microphone(microphone, reason)) << reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;

        trap.arm();
        for (int block = 0; block < 3; ++block)
        {
            node.process(context);
        }
        trap.disarm();

        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        EXPECT_TRUE(std::isfinite(ports.listener(0)[i]));
        EXPECT_TRUE(std::isfinite(ports.microphone(0)[i]));
    }
}

TEST(Small_RoomNode, ProcessPerformsNoHeapAllocationAtTheLimits)
{
    // Three cabinets and eight microphones, with placements coming and going between blocks.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});

    std::vector<float> input(256);
    fill_room_tone(input);
    const float* input_channels[6] = {input.data(), input.data(), input.data(),
                                      input.data(), input.data(), input.data()};
    SushiDSP::test::RoomPorts ports(256);
    SushiDSP::ProcessContext context = ports.context(input_channels, 256);

    SushiDSP::test::AllocationTrap trap;
    const std::uint32_t source_counts[4] = {3, 3, 1, 3};
    const std::uint32_t microphone_counts[4] = {8, 8, 2, 8};
    for (int publish = 0; publish < 4; ++publish)
    {
        SushiDSP::Room::Arrangement arrangement = SushiDSP::test::spread_arrangement(
            SushiDSP::test::garage_scene(), source_counts[publish], microphone_counts[publish]);
        if (publish == 1)
        {
            SushiDSP::Room::MicrophonePlacement moved = arrangement.microphones().front();
            moved.pose.position.x += 0.2;
            ASSERT_TRUE(arrangement.update_microphone(moved));
        }
        std::string reason;
        ASSERT_TRUE(node.set_arrangement(arrangement, reason)) << reason;

        trap.arm();
        for (int block = 0; block < 4; ++block)
        {
            node.process(context);
        }
        trap.disarm();

        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < 256; ++i)
    {
        EXPECT_TRUE(std::isfinite(ports.listener(0)[i]));
        EXPECT_TRUE(std::isfinite(ports.listener(1)[i]));
        EXPECT_TRUE(std::isfinite(ports.microphone(7)[i]));
    }
}

TEST(Small_RoomNode, BinauralProcessPerformsNoHeapAllocation)
{
    // The ears render every binaural path inside the trap. See tests/nodes/room/test_room_node/README.md.
    SushiDSP::room::RoomNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::string reason;
    ASSERT_TRUE(node.set_scene(room_trap_scene(4.0), reason)) << reason;

    std::vector<float> input(256);
    fill_room_tone(input);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    for (int publish = 0; publish < 3; ++publish)
    {
        if (publish == 1)
        {
            node.set_parameter_for_control_thread("direct_delay", 1.0f);
        }
        if (publish == 2)
        {
            ASSERT_TRUE(node.set_hrtf(SushiDSP::room::HrtfSet{}));
        }
        trap.arm();
        for (int block = 0; block < 4; ++block)
        {
            node.set_parameter_for_audio_thread(std::uint16_t{3}, block % 2 == 0 ? 1.0f : 0.0f);
            node.process(context);
        }
        trap.disarm();
        node.reclaim_rooms();
    }

    EXPECT_EQ(trap.count(), 0);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_TRUE(std::isfinite(left[i]));
        EXPECT_TRUE(std::isfinite(right[i]));
    }
}
