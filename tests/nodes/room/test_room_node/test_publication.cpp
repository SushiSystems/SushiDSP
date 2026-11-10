/**************************************************************************/
/* test_publication.cpp                                                   */
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
 * @file test_publication.cpp
 * @brief Tests for RoomNode: its output ports, the refusal of an invalid scene, and the old set
 * the node keeps rendering and reclaims while new sets are published.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include "nodes/room/test_room_node/room_node_rig.hpp"

namespace
{
    using namespace SushiDSP::test::room_node_rig;

    /// @brief Returns the channels the output port of one index carries: the listener's port two,
    /// each microphone's port one, and no port past them none.
    std::uint32_t expected_channels(std::uint32_t port)
    {
        if (port == SushiDSP::room::RoomNode::K_LISTENER_PORT)
        {
            return 2u;
        }
        return port < SushiDSP::room::RoomNode::K_OUTPUT_PORTS ? 1u : 0u;
    }

    /// @brief Holds the publishes the keep-rendering test walks. The swap slot holds the active
    /// set, the one the audio thread read last and the new one, three generations, so eight
    /// publishes take the slot through its cycle more than twice.
    constexpr int K_PUBLISHES = 8;

    /// @brief Holds the blocks rendered after each publish.
    constexpr int K_BLOCKS_PER_PUBLISH = 4;

    /// @brief Holds the quietest block RMS the old set may fall to while a set is built, in dB.
    constexpr double K_AUDIBLE_FLOOR_DB = -80.0;
} // namespace

class Small_RoomNodePortRows : public ::testing::TestWithParam<std::uint32_t>
{
};

TEST_P(Small_RoomNodePortRows, RoomNodeGivesEachReceiverItsOwnOutputPort)
{
    SushiDSP::room::RoomNode node;
    EXPECT_EQ(node.output_port_channels(GetParam()), expected_channels(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(Ports, Small_RoomNodePortRows,
                         ::testing::Range<std::uint32_t>(
                             0u, SushiDSP::room::RoomNode::K_OUTPUT_PORTS + 1u));

TEST(Small_RoomNodePorts, RoomNodeOwnsOnePortForTheListenerAndOnePerMicrophone)
{
    SushiDSP::room::RoomNode node;
    EXPECT_EQ(node.output_port_count(), 1u + SushiDSP::room::RoomNode::K_MAX_MICROPHONES);
}

TEST(Small_RoomNodeScenePublication, RoomNodeRefusesAnInvalidSceneBeforePrepare)
{
    SushiDSP::room::RoomScene invalid = microphone_scene();
    invalid.dimensions.x = 15.0;
    SushiDSP::room::RoomNode fresh;
    const double dimension_before = fresh.scene().dimensions.x;
    std::string reason;
    EXPECT_FALSE(publish_scene(fresh, invalid, reason));
    EXPECT_FALSE(reason.empty());
    EXPECT_EQ(fresh.scene().dimensions.x, dimension_before);
}

TEST(Medium_RoomNodeScenePublication, RoomNodeRefusesAnInvalidSceneAndKeepsTheOldOne)
{
    const SushiDSP::room::RoomScene valid = microphone_scene();
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(valid, K_LISTENER_MICROPHONE);

    SushiDSP::room::RoomScene invalid = valid;
    invalid.dimensions.x = 15.0;
    std::string reason;
    EXPECT_FALSE(publish_scene(*node, invalid, reason));
    EXPECT_FALSE(reason.empty());
    EXPECT_EQ(node->scene().dimensions.x, valid.dimensions.x);
    EXPECT_EQ(node->scene().microphone_position.x, valid.microphone_position.x);
}

TEST(Medium_RoomNodeScenePublication, RoomNodeKeepsRenderingTheOldSetWhileASetIsBuilt)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(microphone_scene(), K_LISTENER_MICROPHONE);
    std::vector<float> left(K_BLOCK, 0.0f);
    std::vector<float> right(K_BLOCK, 0.0f);
    float* outputs[2] = {left.data(), right.data()};
    const std::vector<float> input = tone(static_cast<std::size_t>(K_PUBLISHES) *
                                          K_BLOCKS_PER_PUBLISH * K_BLOCK);

    std::size_t offset = 0;
    bool every_block_finite = true;
    double quietest_rms = 1.0;
    for (int publish = 0; publish < K_PUBLISHES; ++publish)
    {
        SushiDSP::room::RoomScene scene = microphone_scene();
        scene.microphone_position.x = 3.0 + 0.01 * publish;
        std::string reason;
        EXPECT_TRUE(publish_scene(*node, scene, reason)) << reason;
        for (int repeat = 0; repeat < K_BLOCKS_PER_PUBLISH; ++repeat, offset += K_BLOCK)
        {
            const float* inputs[1] = {input.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
            node->process(context);
            every_block_finite = every_block_finite && all_finite(left);
            quietest_rms = std::min(quietest_rms, std::sqrt(energy(left, 0, left.size()) / K_BLOCK));
        }
        node->reclaim_rooms();
    }
    EXPECT_TRUE(every_block_finite);
    EXPECT_GT(to_decibels(quietest_rms), K_AUDIBLE_FLOOR_DB);
}

TEST(Medium_RoomNodeSetReclaim, RoomNodeRendersFiniteAudioAfterEachReclaimOfARetiredSet)
{
    // The reclaim frees a retired set at any time; a sanitizer run is what catches a read of it.
    std::unique_ptr<SushiDSP::room::RoomNode> node =
        prepared_node(microphone_scene(), K_LISTENER_MICROPHONE);
    const std::vector<float> input = tone(4 * K_BLOCK);
    const auto block = [&input](std::size_t index)
    {
        return std::vector<float>(input.begin() + static_cast<std::ptrdiff_t>(index * K_BLOCK),
                                  input.begin() + static_cast<std::ptrdiff_t>((index + 1) * K_BLOCK));
    };

    SushiDSP::room::RoomScene second = microphone_scene();
    second.microphone_position.x = 3.5;
    SushiDSP::room::RoomScene third = microphone_scene();
    third.microphone_position.x = 3.0;

    const std::vector<float> first_block = render(*node, block(0));
    std::string reason;
    EXPECT_TRUE(publish_scene(*node, second, reason)) << reason;
    EXPECT_TRUE(publish_scene(*node, third, reason)) << reason;
    node->reclaim_rooms();
    const std::vector<float> after_reclaim = render(*node, block(1));
    node->reclaim_rooms();
    const std::vector<float> settled = render(*node, block(2));

    EXPECT_TRUE(all_finite(first_block));
    EXPECT_TRUE(all_finite(after_reclaim));
    EXPECT_TRUE(all_finite(settled));
    EXPECT_EQ(node->scene().microphone_position.x, third.microphone_position.x);
}
