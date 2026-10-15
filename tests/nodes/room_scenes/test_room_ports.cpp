/**************************************************************************/
/* test_room_ports.cpp                                                    */
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
 * @file test_room_ports.cpp
 * @brief Tests that the room port table owns its buffers and cannot be copied.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room_scenes/room_ports.hpp>

#include <type_traits>

TEST(RoomPorts, CannotBeCopied)
{
    EXPECT_FALSE(std::is_copy_constructible<SushiDSP::test::RoomPorts>::value);
    EXPECT_FALSE(std::is_copy_assignable<SushiDSP::test::RoomPorts>::value);
}

TEST(RoomPorts, ConstructsForAGivenBlockSize)
{
    const SushiDSP::test::RoomPorts ports(256u);
    (void)ports;
    SUCCEED();
}
