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
 * @file test_room_fixtures.cpp
 * @brief Tests that the three reference rooms carry the material and scattering the acceptance tests assume.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

TEST(RoomFixtures, GarageIsConcreteAllRoundWithFlatScattering)
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

TEST(RoomFixtures, BlockRoomIsPaintedConcreteBlockAllRound)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::block_room_scene();
    for (const SushiDSP::room::MaterialId material : scene.surfaces)
    {
        EXPECT_EQ(material, SushiDSP::room::MaterialId::ConcreteBlockPainted);
    }
}

TEST(RoomFixtures, BedroomFloorIsNotTheGarageConcrete)
{
    const SushiDSP::room::RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const std::size_t floor = static_cast<std::size_t>(SushiDSP::room::Surface::Floor);
    EXPECT_NE(bedroom.surfaces[floor], SushiDSP::room::MaterialId::ConcreteUnpainted);
}
