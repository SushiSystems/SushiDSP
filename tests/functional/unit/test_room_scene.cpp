/**************************************************************************/
/* test_room_scene.cpp                                                    */
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
 * @file test_room_scene.cpp
 * @brief Tests for RoomScene, the shoebox, its materials, its patches and its three poses.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <string>

#include "RoomFixtures.hpp"
#include "SushiDSP/room/BuiltRoomLoss.hpp"
#include "SushiDSP/room/RoomScene.hpp"

TEST(Unit_Dsp, RoomSceneComputesTheGaragesVolumeAndArea)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_NEAR(garage.volume_m3(), 60.0, 1e-9);
    EXPECT_NEAR(garage.surface_area_m2(), 98.0, 1e-9);
}

TEST(Unit_Dsp, RoomSceneWeightsAPatchByItsArea)
{
    // 92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2:
    // 125 Hz (92 * 0.01 + 6 * 1.0) / 98 = 0.070612; 500 Hz (92 * 0.02 + 6 * 1.0) / 98 = 0.080000.
    const std::array<double, 6> absorption = SushiDSP::test::garage_scene().mean_absorption();
    EXPECT_NEAR(absorption[0], 0.070612, 1e-6);
    EXPECT_NEAR(absorption[2], 0.080000, 1e-6);
}

TEST(Unit_Dsp, RoomSceneComputesTheBedroomsWeightedAbsorption)
{
    // 14 m2 carpet, 51.5 m2 gypsum, 1.5 m2 glass over 67 m2:
    // 500 Hz (14 * 0.14 + 51.5 * 0.05 + 1.5 * 0.18) / 67 = 0.071716.
    EXPECT_NEAR(SushiDSP::test::bedroom_scene().mean_absorption()[2], 0.071716, 1e-6);
}

TEST(Unit_Dsp, RoomSceneRefusesADimensionOutsideItsBounds)
{
    using SushiDSP::room::RoomScene;

    RoomScene too_small = SushiDSP::test::garage_scene();
    too_small.dimensions.x = 1.0;
    std::string reason;
    EXPECT_FALSE(too_small.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);

    RoomScene too_large = SushiDSP::test::garage_scene();
    too_large.dimensions.x = 15.0;
    reason.clear();
    EXPECT_FALSE(too_large.is_valid(reason));
    EXPECT_NE(reason.find('x'), std::string::npos);
    EXPECT_NE(reason.find("1.5"), std::string::npos);
    EXPECT_NE(reason.find("12"), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneRefusesAPoseOutsideTheRoom)
{
    using SushiDSP::room::RoomScene;

    RoomScene behind_wall = SushiDSP::test::garage_scene();
    behind_wall.source_position.x = -0.1;
    std::string reason;
    EXPECT_FALSE(behind_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);

    RoomScene past_wall = SushiDSP::test::garage_scene();
    past_wall.source_position.x = 6.1;
    reason.clear();
    EXPECT_FALSE(past_wall.is_valid(reason));
    EXPECT_NE(reason.find("source"), std::string::npos);
    EXPECT_NE(reason.find('x'), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneRefusesAPatchLargerThanItsFace)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patches[0] = { Surface::PlusX, MaterialId::OpenDoorway, 12.0 };
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("12"), std::string::npos);
    EXPECT_NE(reason.find("10"), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneAcceptsEveryReferenceRoom)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    // The default scene is 6.0 x 4.0 x 2.5 m of gypsum board on studs over a carpeted floor:
    // 74 m2 of board and 24 m2 of carpet over 98 m2 give (74 * 0.29 + 24 * 0.02) / 98 = 0.223878
    // at 125 Hz and (74 * 0.05 + 24 * 0.14) / 98 = 0.072041 at 500 Hz, its least absorbent band,
    // which clears the 0.008181 floor this shoebox needs by a factor of 8.8.
    std::string reason;
    const RoomScene fresh;
    EXPECT_TRUE(fresh.is_valid(reason)) << reason;
    EXPECT_NEAR(fresh.min_mean_absorption(), 0.008181, 1e-6);
    EXPECT_NEAR(fresh.mean_absorption()[0], 0.223878, 1e-6);
    EXPECT_NEAR(fresh.mean_absorption()[2], 0.072041, 1e-6);

    EXPECT_TRUE(SushiDSP::test::garage_scene().is_valid(reason)) << reason;
    EXPECT_TRUE(SushiDSP::test::bedroom_scene().is_valid(reason)) << reason;
    EXPECT_TRUE(SushiDSP::test::block_room_scene().is_valid(reason)) << reason;

    // The 12 m carpeted room with four open faces the node tests render, whose absorption runs
    // 0.673 at 125 Hz against a floor of 0.026477.
    RoomScene dead;
    dead.dimensions = { 12.0, 12.0, 12.0 };
    dead.surfaces.fill(MaterialId::HeavyCarpetOnConcrete);
    dead.patches[0] = { Surface::MinusX, MaterialId::OpenDoorway, 144.0 };
    dead.patches[1] = { Surface::PlusX, MaterialId::OpenDoorway, 144.0 };
    dead.patches[2] = { Surface::MinusY, MaterialId::OpenDoorway, 144.0 };
    dead.patches[3] = { Surface::PlusY, MaterialId::OpenDoorway, 144.0 };
    dead.patch_count = 4;
    dead.source_position = { 3.0, 4.0, 1.0 };
    dead.listener_position = { 7.0, 5.0, 1.5 };
    dead.microphone_position = { 7.0, 5.0, 1.5 };
    EXPECT_TRUE(dead.is_valid(reason)) << reason;
    EXPECT_NEAR(dead.min_mean_absorption(), 0.026477, 1e-6);

    // The 3.0 x 2.5 x 2.4 m unpainted concrete room the crossover case walks: 6.96 s at 125 Hz.
    RoomScene small_concrete;
    small_concrete.dimensions = { 3.0, 2.5, 2.4 };
    small_concrete.surfaces.fill(MaterialId::ConcreteUnpainted);
    small_concrete.source_position = { 0.3, 0.3, 0.3 };
    small_concrete.listener_position = { 1.5, 1.25, 1.2 };
    small_concrete.microphone_position = { 1.5, 1.25, 1.2 };
    EXPECT_TRUE(small_concrete.is_valid(reason)) << reason;
    EXPECT_NEAR(small_concrete.min_mean_absorption(), 0.005816, 1e-6);
}

TEST(Unit_Dsp, RoomSceneRefusesARoomThatRingsPastTheModulesDecay)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;

    // The 12 m cube of unpainted concrete: 0.010000 at 125 Hz against a floor of 0.026477,
    // Eyring's 32.04 s from its surfaces alone against the 12 s bound.
    RoomScene cube;
    cube.dimensions = { 12.0, 12.0, 12.0 };
    cube.surfaces.fill(MaterialId::ConcreteUnpainted);
    std::string reason;
    EXPECT_FALSE(cube.is_valid(reason));
    EXPECT_NE(reason.find("0.010000"), std::string::npos) << reason;
    EXPECT_NE(reason.find("125"), std::string::npos) << reason;
    EXPECT_NE(reason.find("0.026477"), std::string::npos) << reason;
    EXPECT_NE(reason.find("12 s"), std::string::npos) << reason;

    // The bound is on every band, not on the mid one: a 10 m cube of gypsum board on studs is
    // dead at 125 Hz, 0.29, and rings 6.57 s at 1 kHz, so a room whose bass is fine and whose
    // mid is not stays valid while the same shell in concrete does not.
    RoomScene gypsum;
    gypsum.dimensions = { 10.0, 10.0, 10.0 };
    gypsum.surfaces.fill(MaterialId::GypsumBoardOnStuds);
    EXPECT_TRUE(gypsum.is_valid(reason)) << reason;

    // A bare unpainted concrete cube crosses the bound at an edge of 4.495 m, where 0.161 L / 6
    // over -ln(1 - 0.01) reaches 12 s; 4.4 m gives 11.75 s and 4.6 m gives 12.28 s.
    RoomScene under;
    under.dimensions = { 4.4, 4.4, 4.4 };
    under.surfaces.fill(MaterialId::ConcreteUnpainted);
    EXPECT_TRUE(under.is_valid(reason)) << reason;

    RoomScene over;
    over.dimensions = { 4.6, 4.6, 4.6 };
    over.surfaces.fill(MaterialId::ConcreteUnpainted);
    EXPECT_FALSE(over.is_valid(reason));

    // Absorption alone does not decide it: the same concrete at 6.0 x 4.0 x 2.5 m passes, since
    // the floor rises with the room's volume over its area.
    RoomScene shoebox;
    shoebox.dimensions = { 6.0, 4.0, 2.5 };
    shoebox.surfaces.fill(MaterialId::ConcreteUnpainted);
    EXPECT_TRUE(shoebox.is_valid(reason)) << reason;
    EXPECT_LT(shoebox.min_mean_absorption(), over.min_mean_absorption());
}

TEST(Unit_Dsp, RoomSceneSumsItsFurnishingAreaByQuantity)
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::furniture;

    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = { FurnitureId::WardrobeModule, 3.0 };
    scene.furnishings[1] = { FurnitureId::SofaTwoSeat, 2.0 };
    scene.furnishing_count = 2;

    const std::array<double, 6> area = scene.furnishing_absorption_area();
    const auto& wardrobe = furniture(FurnitureId::WardrobeModule).absorption_area;
    const auto& sofa = furniture(FurnitureId::SofaTwoSeat).absorption_area;
    for (std::size_t band = 0; band < area.size(); ++band)
    {
        const double expected = 3.0 * static_cast<double>(wardrobe[band])
            + 2.0 * static_cast<double>(sofa[band]);
        EXPECT_NEAR(area[band], expected, 1e-12) << "band " << band;
    }
}

TEST(Unit_Dsp, RoomSceneWithoutFurnishingsHasNoFurnishingArea)
{
    const std::array<double, 6> area = SushiDSP::test::garage_scene().furnishing_absorption_area();
    for (std::size_t band = 0; band < area.size(); ++band)
    {
        EXPECT_EQ(area[band], 0.0) << "band " << band;
    }
}

TEST(Unit_Dsp, RoomSceneRefusesABadFurnishing)
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::RoomScene;

    const auto refused_with = [](double quantity)
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.furnishings[0] = { FurnitureId::SofaTwoSeat, quantity };
        scene.furnishing_count = 1;
        std::string reason;
        EXPECT_FALSE(scene.is_valid(reason)) << "quantity " << quantity;
        EXPECT_FALSE(reason.empty()) << "quantity " << quantity;
    };
    refused_with(0.0);
    refused_with(-1.0);
    refused_with(std::numeric_limits<double>::quiet_NaN());
    refused_with(std::numeric_limits<double>::infinity());

    RoomScene unknown_item = SushiDSP::test::garage_scene();
    unknown_item.furnishings[0] = { static_cast<FurnitureId>(200), 1.0 };
    unknown_item.furnishing_count = 1;
    std::string reason;
    EXPECT_FALSE(unknown_item.is_valid(reason));
    EXPECT_FALSE(reason.empty());

    RoomScene too_many = SushiDSP::test::garage_scene();
    too_many.furnishing_count = 17;
    reason.clear();
    EXPECT_FALSE(too_many.is_valid(reason));
    EXPECT_NE(reason.find("16"), std::string::npos) << reason;
}

TEST(Unit_Dsp, RoomSceneCountsFurnitureTowardTheDecayBound)
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;

    // A 12 m concrete cube rings past the bound bare and inside it with six sofas.
    RoomScene cube;
    cube.dimensions = { 12.0, 12.0, 12.0 };
    cube.surfaces.fill(MaterialId::ConcreteUnpainted);
    std::string reason;
    EXPECT_FALSE(cube.is_valid(reason));

    cube.furnishings[0] = { FurnitureId::SofaTwoSeat, 6.0 };
    cube.furnishing_count = 1;
    EXPECT_TRUE(cube.is_valid(reason)) << reason;
}

TEST(Unit_Dsp, RoomSceneKeepsItsAbsorptionWhenFurnished)
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::RoomScene;

    const RoomScene bare = SushiDSP::test::bedroom_scene();
    RoomScene furnished = bare;
    furnished.furnishings[0] = { FurnitureId::SofaTwoSeat, 2.0 };
    furnished.furnishing_count = 1;

    const std::array<double, 6> before = bare.mean_absorption();
    const std::array<double, 6> after = furnished.mean_absorption();
    for (std::size_t band = 0; band < before.size(); ++band)
    {
        EXPECT_EQ(before[band], after[band]) << "band " << band;
    }
}

TEST(Unit_Dsp, RoomSceneRefusesPatchesThatTogetherOverfillAFace)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patch_count = 2;
    garage.patches[0] = { Surface::PlusY, MaterialId::GlassWindow, 8.0 };
    garage.patches[1] = { Surface::PlusY, MaterialId::WoodenDoor, 8.0 };
    std::string reason;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("total"), std::string::npos);

    garage.patches[1].surface = Surface::MinusY;
    EXPECT_TRUE(garage.is_valid(reason)) << reason;
}

TEST(Unit_Dsp, RoomSceneRefusesABadPatchAreaAndTooManyPatches)
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    RoomScene garage = SushiDSP::test::garage_scene();
    garage.patch_count = 1;
    std::string reason;
    for (const double area : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity() })
    {
        garage.patches[0] = { Surface::PlusX, MaterialId::GlassWindow, area };
        EXPECT_FALSE(garage.is_valid(reason)) << "area " << area;
        EXPECT_FALSE(reason.empty());
    }

    garage.patches[0] = { Surface::PlusX, MaterialId::GlassWindow, 1.0 };
    garage.patch_count = RoomScene::kMaxPatches + 1;
    EXPECT_FALSE(garage.is_valid(reason));
    EXPECT_NE(reason.find("patches"), std::string::npos);
}

TEST(Unit_Dsp, RoomSceneAddsTheBuiltRoomLossToEveryFaceWhenSet)
{
    SushiDSP::room::RoomScene plain;
    SushiDSP::room::RoomScene built = plain;
    built.built_room_loss = true;

    for (std::size_t face = 0; face < SushiDSP::room::kSurfaceCount; ++face)
    {
        const auto surface = static_cast<SushiDSP::room::Surface>(face);
        const std::array<double, 6> without = plain.face_absorption(surface);
        const std::array<double, 6> with = built.face_absorption(surface);
        for (std::size_t band = 0; band < 6; ++band)
        {
            EXPECT_DOUBLE_EQ(with[band], without[band] + SushiDSP::room::kBuiltRoomLoss[band])
                << "face " << face << " band " << band;
        }
    }
}

TEST(Unit_Dsp, RoomSceneCapsTheBuiltRoomLossAtFullAbsorption)
{
    SushiDSP::room::RoomScene scene;
    scene.surfaces.fill(SushiDSP::room::MaterialId::OpenDoorway);
    scene.built_room_loss = true;

    const std::array<double, 6> absorption = scene.mean_absorption();
    for (std::size_t band = 0; band < 6; ++band)
    {
        EXPECT_DOUBLE_EQ(absorption[band], 1.0) << "band " << band;
    }
}
