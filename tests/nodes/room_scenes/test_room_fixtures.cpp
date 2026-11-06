/**************************************************************************/
/* test_room_fixtures.cpp                                                 */
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
 * @file test_room_fixtures.cpp
 * @brief Tests that the three reference rooms carry the material and scattering the acceptance tests assume.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

TEST(Small_RoomFixtures, GarageIsConcreteAllRoundWithFlatScattering)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    for (const SushiDSP::room::MaterialId material : scene.surfaces)
    {
        EXPECT_EQ(material, SushiDSP::room::MaterialId::ConcreteUnpainted);
    }
    for (const SushiDSP::room::Scattering scattering : scene.scattering)
    {
        EXPECT_EQ(scattering, SushiDSP::room::Scattering::Flat);
    }
}

TEST(Small_RoomFixtures, BlockRoomIsPaintedConcreteBlockAllRound)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::block_room_scene();
    for (const SushiDSP::room::MaterialId material : scene.surfaces)
    {
        EXPECT_EQ(material, SushiDSP::room::MaterialId::ConcreteBlockPainted);
    }
}

TEST(Small_RoomFixtures, BedroomFloorIsNotTheGarageConcrete)
{
    const SushiDSP::room::RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const std::size_t floor = static_cast<std::size_t>(SushiDSP::room::Surface::Floor);
    EXPECT_NE(bedroom.surfaces[floor], SushiDSP::room::MaterialId::ConcreteUnpainted);
}
