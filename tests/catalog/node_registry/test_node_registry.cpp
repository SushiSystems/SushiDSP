/**************************************************************************/
/* test_node_registry.cpp                                                 */
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
 * @file test_node_registry.cpp
 * @brief Tests that NodeRegistry lists and builds the registered nodes.
 * @author Mustafa Garip
 */

#include <cstring>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/routing/mixer_node.hpp>
#include <SushiDSP/catalog/node_registry/node_registry.hpp>

TEST(Small_NodeRegistry, NodeRegistryListsTubeScreamerAndJcm800)
{
    const auto& entries = SushiDSP::node_registry();
    ASSERT_GE(entries.size(), 2u);

    bool found_tube_screamer = false, found_jcm800 = false;
    for (const auto& entry : entries)
    {
        if (std::strcmp(entry.descriptor->id, "sushidsp.tubescreamer") == 0)
        {
            found_tube_screamer = true;
            EXPECT_STREQ(entry.category.name, "Distortion");
        }
        if (std::strcmp(entry.descriptor->id, "sushidsp.jcm800") == 0)
        {
            found_jcm800 = true;
            EXPECT_STREQ(entry.category.name, "Amp");
        }
    }
    EXPECT_TRUE(found_tube_screamer);
    EXPECT_TRUE(found_jcm800);
}

TEST(Small_NodeRegistry, MakeNodeConstructsTubeScreamerById)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    auto node = SushiDSP::make_node("sushidsp.tubescreamer", info);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->ports().input_channels, 1u);
    EXPECT_EQ(node->ports().output_channels, 1u);
    node->set_parameter_for_control_thread("drive", 0.3f);
    EXPECT_FLOAT_EQ(node->get_parameter("drive"), 0.3f);
}

TEST(Small_NodeRegistry, MakeNodeConstructsJcm800ById)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    auto node = SushiDSP::make_node("sushidsp.jcm800", info);
    ASSERT_NE(node, nullptr);
    node->set_parameter_for_control_thread("preamp", 0.4f);
    EXPECT_FLOAT_EQ(node->get_parameter("preamp"), 0.4f);
}

TEST(Small_NodeRegistry, NodeRegistryListsAndBuildsTheThreeHighGainAmplifiers)
{
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    static const char* K_AMPLIFIER_IDS[] = {"sushidsp.peavey5150", "sushidsp.mesa_mark2c",
                                            "sushidsp.diezel_vh4"};

    for (const char* id : K_AMPLIFIER_IDS)
    {
        bool found = false;
        for (const auto& entry : SushiDSP::node_registry())
        {
            if (std::strcmp(entry.descriptor->id, id) == 0)
            {
                found = true;
                EXPECT_STREQ(entry.category.name, "Amp");
            }
        }
        EXPECT_TRUE(found) << id;

        auto node = SushiDSP::make_node(id, info);
        ASSERT_NE(node, nullptr) << id;
        EXPECT_EQ(node->ports().input_channels, 1u);
        EXPECT_EQ(node->ports().output_channels, 1u);
    }
}

TEST(Small_NodeRegistry, MakeNodeReturnsNullForUnknownId)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    EXPECT_EQ(SushiDSP::make_node("does.not.exist", info), nullptr);
}

TEST(Small_NodeRegistry, NodeRegistryIncludesRoutingNodes)
{
    const SushiDSP::PrepareInfo info{48000.0, 64, 1};

    auto splitter = SushiDSP::make_node("sushidsp.splitter", info);
    ASSERT_NE(splitter, nullptr);
    EXPECT_EQ(splitter->ports().output_channels, 1u);

    auto mixer = SushiDSP::make_node("sushidsp.mixer", info);
    ASSERT_NE(mixer, nullptr);
    EXPECT_EQ(mixer->ports().input_channels, SushiDSP::MixerNode::K_MAX_FAN_IN);

    bool found_routing_category = false;
    for (const auto& entry : SushiDSP::node_registry())
    {
        if (std::string(entry.category.name) == "Routing")
        {
            found_routing_category = true;
        }
    }
    EXPECT_TRUE(found_routing_category);
}

TEST(Small_NodeRegistry, NodeRegistryBuildsTheCabinet)
{
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    auto node = SushiDSP::make_node("sushidsp.cabinet", info);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), "sushidsp.cabinet");
}

TEST(Small_NodeRegistry, NodeRegistryListsTheRoomUnderRoomAndBuildsIt)
{
    bool found = false;
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        if (std::strcmp(entry.descriptor->id, "sushidsp.room") == 0)
        {
            found = true;
            EXPECT_STREQ(entry.category.name, "Room");
            EXPECT_EQ(entry.descriptor->parameter_count, 1u);
            EXPECT_STREQ(entry.descriptor->parameters[0].id, "direct_delay");
            EXPECT_FLOAT_EQ(entry.descriptor->parameters[0].default_value, 0.0f);
        }
    }
    EXPECT_TRUE(found);

    std::unique_ptr<SushiDSP::AudioNode> node =
        SushiDSP::make_node("sushidsp.room", SushiDSP::PrepareInfo{48000.0, 256, 1});
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->ports().input_channels, 6u);
    EXPECT_EQ(node->ports().output_channels, 2u);
    EXPECT_FLOAT_EQ(node->get_parameter("direct_delay"), 0.0f);
    node->set_parameter_for_control_thread("direct_delay", 1.0f);
    EXPECT_FLOAT_EQ(node->get_parameter("direct_delay"), 1.0f);
    EXPECT_NE(dynamic_cast<SushiDSP::room::RoomHost*>(node.get()), nullptr);
}