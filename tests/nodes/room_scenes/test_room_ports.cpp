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
 * @file test_room_ports.cpp
 * @brief Tests that the room port table owns its buffers and cannot be copied.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room_scenes/room_ports.hpp>

#include <type_traits>

TEST(Small_RoomPorts, CannotBeCopied)
{
    EXPECT_FALSE(std::is_copy_constructible<SushiDSP::test::RoomPorts>::value);
    EXPECT_FALSE(std::is_copy_assignable<SushiDSP::test::RoomPorts>::value);
}

TEST(Small_RoomPorts, ConstructsForAGivenBlockSize)
{
    const SushiDSP::test::RoomPorts ports(256u);
    (void)ports;
    SUCCEED();
}
