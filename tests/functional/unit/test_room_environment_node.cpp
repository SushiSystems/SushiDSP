/**************************************************************************/
/* test_room_environment_node.cpp                                         */
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

/** @file test_room_environment_node.cpp
 *  @brief Tests the room environment's port layout and its refusals of out-of-range counts.
 *  @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/room/room_environment_node.hpp"

using SushiDSP::Room::RoomEnvironmentNode;

TEST(Unit_Dsp, RoomEnvironmentDeclaresOneMonoInputPerCabinetSlot)
{
    RoomEnvironmentNode room;
    const SushiDSP::PortLayout layout = room.ports();

    EXPECT_EQ(layout.input_channels, RoomEnvironmentNode::K_MAX_SOURCES);
    EXPECT_EQ(layout.output_channels, 1u);
}

TEST(Unit_Dsp, RoomEnvironmentDeclaresTwoEarsAndOnePortPerMicrophone)
{
    RoomEnvironmentNode room;

    EXPECT_EQ(room.output_port_count(), RoomEnvironmentNode::K_OUTPUT_PORTS);
    EXPECT_EQ(RoomEnvironmentNode::K_OUTPUT_PORTS, 10u);
    for (std::uint32_t port = 0; port < room.output_port_count(); ++port)
    {
        EXPECT_EQ(room.output_port_channels(port), 1u) << "port " << port;
    }
    EXPECT_EQ(room.output_port_channels(RoomEnvironmentNode::K_OUTPUT_PORTS), 0u);
}

TEST(Unit_Dsp, RoomEnvironmentRefusesMoreCabinetsThanItsSlots)
{
    RoomEnvironmentNode room;
    const std::vector<RoomEnvironmentNode::Placement> sources(
        RoomEnvironmentNode::K_MAX_SOURCES + 1);
    std::string reason;

    EXPECT_FALSE(room.configure(SushiDSP::room::RoomScene{}, sources,
                                RoomEnvironmentNode::Placement{}, {}, reason));
    EXPECT_NE(reason.find("cabinets"), std::string::npos) << reason;
}

TEST(Unit_Dsp, RoomEnvironmentRefusesMoreMicrophonesThanItsPorts)
{
    RoomEnvironmentNode room;
    const std::vector<RoomEnvironmentNode::Microphone> microphones(
        RoomEnvironmentNode::K_MAX_MICROPHONES + 1);
    std::string reason;

    EXPECT_FALSE(room.configure(SushiDSP::room::RoomScene{}, {},
                                RoomEnvironmentNode::Placement{}, microphones, reason));
    EXPECT_NE(reason.find("microphones"), std::string::npos) << reason;
}

TEST(Unit_Dsp, RoomEnvironmentReportsItsTypeId)
{
    RoomEnvironmentNode room;

    EXPECT_STREQ(room.type_id(), "sushidsp.room_environment");
}
