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

#include <cstddef>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

namespace
{
    /// Returns an array of the given type with every element set to one value.
    /// @tparam Array A std::array type.
    /// @param value The value of every element.
    template <typename Array>
    Array filled_with(typename Array::value_type value)
    {
        Array array;
        array.fill(value);
        return array;
    }
} // namespace

TEST(Small_RoomFixtures, GarageIsConcreteAllRoundWithFlatScattering)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();

    EXPECT_EQ(scene.surfaces, filled_with<decltype(scene.surfaces)>(SushiDSP::room::MaterialId::ConcreteUnpainted));
    EXPECT_EQ(scene.scattering, filled_with<decltype(scene.scattering)>(SushiDSP::room::Scattering::Flat));
}

TEST(Small_RoomFixtures, BlockRoomIsPaintedConcreteBlockAllRound)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::block_room_scene();

    EXPECT_EQ(scene.surfaces,
              filled_with<decltype(scene.surfaces)>(SushiDSP::room::MaterialId::ConcreteBlockPainted));
}

TEST(Small_RoomFixtures, BedroomFloorIsNotTheGarageConcrete)
{
    const SushiDSP::room::RoomScene bedroom = SushiDSP::test::bedroom_scene();
    const std::size_t floor = static_cast<std::size_t>(SushiDSP::room::Surface::Floor);
    EXPECT_NE(bedroom.surfaces[floor], SushiDSP::room::MaterialId::ConcreteUnpainted);
}
