/**************************************************************************/
/* test_room_scene_furnishings.cpp                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_room_scene_furnishings.cpp
 * @brief Tests for the furnishing seam of RoomScene: its decay bound, its count and the
 * entries past the count.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/Furniture.hpp"
#include "SushiDSP/room/MediumAbsorption.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::kBandCount;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;

    /// @brief Builds the 12 m cube of unpainted concrete whose bare surfaces sit under the bound.
    RoomScene concrete_cube()
    {
        RoomScene cube;
        cube.dimensions = { 12.0, 12.0, 12.0 };
        cube.surfaces.fill(MaterialId::ConcreteUnpainted);
        return cube;
    }

    /// @brief Builds a scene of the cube holding one entry of the given item and quantity.
    RoomScene cube_with_sofas(double quantity)
    {
        RoomScene scene = concrete_cube();
        scene.furnishings[0] = { FurnitureId::SofaTwoSeat, quantity };
        scene.furnishing_count = 1;
        return scene;
    }

    /// @brief Returns the absorption area one item of the row gives at one band.
    double row_area(FurnitureId item, std::size_t band)
    {
        return static_cast<double>(SushiDSP::room::furniture(item).absorption_area[band]);
    }

    /// @brief Returns the area the exact bound demands at one band,
    /// S * -ln((1 - floor) / (1 - a_s)).
    double exact_required_area(const RoomScene& scene, std::size_t band)
    {
        const double floor_absorption = scene.min_mean_absorption();
        const double surface_absorption = scene.mean_absorption()[band];
        return scene.surface_area_m2()
            * -std::log((1.0 - floor_absorption) / (1.0 - surface_absorption));
    }

    /// @brief Returns the area the linear form S * (floor - a_s) would demand at one band.
    double linear_required_area(const RoomScene& scene, std::size_t band)
    {
        return scene.surface_area_m2()
            * (scene.min_mean_absorption() - scene.mean_absorption()[band]);
    }
}

TEST(Unit_Dsp, RoomSceneBoundUsesTheExactFormNotTheLinearOne)
{
    const RoomScene cube = concrete_cube();
    const double per_sofa = row_area(FurnitureId::SofaTwoSeat, 0);
    const double exact_quantity = exact_required_area(cube, 0) / per_sofa;
    const double linear_quantity = linear_required_area(cube, 0) / per_sofa;

    const double under = exact_quantity * 0.99;
    const double over = exact_quantity * 1.01;
    ASSERT_GT(under, linear_quantity) << "the refused quantity must clear the linear form";
    ASSERT_LT(linear_quantity, exact_quantity);

    std::string reason;
    EXPECT_FALSE(cube_with_sofas(under).is_valid(reason)) << "quantity " << under;
    EXPECT_FALSE(reason.empty());
    EXPECT_TRUE(cube_with_sofas(over).is_valid(reason)) << "quantity " << over << ": " << reason;

    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        EXPECT_GT(over * row_area(FurnitureId::SofaTwoSeat, band), exact_required_area(cube, band))
            << "band " << band;
    }
}

TEST(Unit_Dsp, RoomSceneAcceptsExactlyTheMaximumFurnishings)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    for (std::size_t index = 0; index < RoomScene::kMaxFurnishings; ++index)
    {
        scene.furnishings[index] = { FurnitureId::BedDouble, 0.05 };
    }
    scene.furnishing_count = static_cast<std::uint32_t>(RoomScene::kMaxFurnishings);
    std::string reason;
    EXPECT_TRUE(scene.is_valid(reason)) << reason;

    scene.furnishing_count = static_cast<std::uint32_t>(RoomScene::kMaxFurnishings + 1);
    EXPECT_FALSE(scene.is_valid(reason));
}

TEST(Unit_Dsp, RoomSceneRefusesABadFurnishingAfterAGoodOne)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = { FurnitureId::SofaTwoSeat, 1.0 };
    scene.furnishings[1] = { FurnitureId::BedDouble, 0.0 };
    scene.furnishing_count = 2;
    std::string reason;
    EXPECT_FALSE(scene.is_valid(reason));
    EXPECT_NE(reason.find("furnishing 1 "), std::string::npos) << reason;
}

TEST(Unit_Dsp, RoomSceneAcceptsEveryReferenceRoomUnchangedWhenUnfurnished)
{
    const std::array<RoomScene, 3> rooms = { SushiDSP::test::garage_scene(),
                                             SushiDSP::test::bedroom_scene(),
                                             SushiDSP::test::block_room_scene() };
    for (std::size_t room = 0; room < rooms.size(); ++room)
    {
        RoomScene scene = rooms[room];
        for (RoomScene::Furnishing& entry : scene.furnishings)
        {
            entry = { static_cast<FurnitureId>(200), -1.0 };
        }
        scene.furnishing_count = 0;
        std::string reason;
        EXPECT_TRUE(scene.is_valid(reason)) << "room " << room << ": " << reason;
    }
}

TEST(Unit_Dsp, FurnishingAreaIgnoresEntriesBeyondTheCount)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = { FurnitureId::SofaTwoSeat, 1.0 };
    scene.furnishings[1] = { FurnitureId::BedDouble, 1.0 };
    scene.furnishing_count = 1;
    std::array<double, kBandCount> area = scene.furnishing_absorption_area();
    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        EXPECT_EQ(area[band], row_area(FurnitureId::SofaTwoSeat, band)) << "band " << band;
    }

    for (RoomScene::Furnishing& entry : scene.furnishings)
    {
        entry = { FurnitureId::SofaTwoSeat, 1.0 };
    }
    scene.furnishing_count = static_cast<std::uint32_t>(RoomScene::kMaxFurnishings + 5);
    area = scene.furnishing_absorption_area();
    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        EXPECT_DOUBLE_EQ(area[band],
                         static_cast<double>(RoomScene::kMaxFurnishings)
                             * row_area(FurnitureId::SofaTwoSeat, band))
            << "band " << band;
    }
}

TEST(Unit_Dsp, MediumAbsorptionIgnoresALiveLookingEntryPastTheCount)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = { FurnitureId::SofaTwoSeat, 1.0 };
    scene.furnishings[1] = { FurnitureId::WardrobeModule, 3.0 };
    scene.furnishing_count = 1;

    const SushiDSP::room::MediumAbsorption medium(scene);
    const double four_volumes = 4.0 * scene.volume_m3();
    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        EXPECT_NEAR(medium.furnishing_coefficient_per_metre(band),
                    row_area(FurnitureId::SofaTwoSeat, band) / four_volumes, 1e-12)
            << "band " << band;
    }
}
