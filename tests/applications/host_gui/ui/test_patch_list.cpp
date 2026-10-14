/**************************************************************************/
/* test_patch_list.cpp                                                    */
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
 * @file test_patch_list.cpp
 * @brief Tests for the pure patch-list edits the room panel calls.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <SushiDSP/nodes/room/materials.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "ui/patch_list.hpp"

namespace
{
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    /// @brief Builds a scene holding count patches whose areas read 1, 2, 3 and so on.
    RoomScene scene_with(std::uint32_t count)
    {
        RoomScene scene;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            EXPECT_TRUE(ui::add_patch(scene, Surface::PlusX, MaterialId::GlassWindow,
                                      static_cast<double>(index + 1)));
        }
        return scene;
    }
} // namespace

TEST(PatchList, PatchListAddFillsToTheLimitAndRefusesTheNext)
{
    RoomScene scene;
    for (std::size_t index = 0; index < RoomScene::K_MAX_PATCHES; ++index)
    {
        ASSERT_TRUE(ui::add_patch(scene, Surface::PlusX, MaterialId::GlassWindow,
                                  static_cast<double>(index + 1)));
        EXPECT_EQ(scene.patch_count, index + 1);
    }
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);

    EXPECT_FALSE(ui::add_patch(scene, Surface::Floor, MaterialId::OpenDoorway, 9.0));
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);
    for (std::size_t index = 0; index < RoomScene::K_MAX_PATCHES; ++index)
    {
        EXPECT_EQ(scene.patches[index].surface, Surface::PlusX);
        EXPECT_EQ(scene.patches[index].material, MaterialId::GlassWindow);
        EXPECT_DOUBLE_EQ(scene.patches[index].area_m2, static_cast<double>(index + 1));
    }
}

TEST(PatchList, PatchListAddRecordsTheFaceTheMaterialAndTheArea)
{
    RoomScene scene;
    ASSERT_TRUE(ui::add_patch(scene, Surface::Ceiling, MaterialId::OpenDoorway, 3.5));
    ASSERT_EQ(scene.patch_count, 1u);
    EXPECT_EQ(scene.patches[0].surface, Surface::Ceiling);
    EXPECT_EQ(scene.patches[0].material, MaterialId::OpenDoorway);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 3.5);
}

TEST(PatchList, PatchListAddRefusesZeroNegativeNaNAndInfinity)
{
    const double refused[] = {0.0,
                              -0.0,
                              -1.0,
                              std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()};
    RoomScene scene = scene_with(2);
    for (double area : refused)
    {
        EXPECT_FALSE(ui::add_patch(scene, Surface::PlusX, MaterialId::GlassWindow, area));
        EXPECT_EQ(scene.patch_count, 2u);
        EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 1.0);
        EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 2.0);
        EXPECT_DOUBLE_EQ(scene.patches[2].area_m2, 0.0);
    }
}

TEST(PatchList, PatchListRemoveShiftsTheTailDownAndKeepsTheOrder)
{
    RoomScene scene = scene_with(4);
    ui::remove_patch(scene, 1);

    ASSERT_EQ(scene.patch_count, 3u);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 1.0);
    EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 3.0);
    EXPECT_DOUBLE_EQ(scene.patches[2].area_m2, 4.0);
    EXPECT_DOUBLE_EQ(scene.patches[3].area_m2, 0.0);
    EXPECT_EQ(scene.patches[3].surface, Surface::MinusX);
    EXPECT_EQ(scene.patches[3].material, static_cast<MaterialId>(0));

    ui::remove_patch(scene, 2);
    ASSERT_EQ(scene.patch_count, 2u);
    EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 3.0);

    ui::remove_patch(scene, 0);
    ASSERT_EQ(scene.patch_count, 1u);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 3.0);
}

TEST(PatchList, PatchListRemoveIgnoresAnIndexAtOrPastTheCount)
{
    RoomScene scene = scene_with(3);
    ui::remove_patch(scene, 3);
    ui::remove_patch(scene, 4);
    ui::remove_patch(scene, std::numeric_limits<std::uint32_t>::max());

    ASSERT_EQ(scene.patch_count, 3u);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 1.0);
    EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 2.0);
    EXPECT_DOUBLE_EQ(scene.patches[2].area_m2, 3.0);

    RoomScene empty;
    ui::remove_patch(empty, 0);
    EXPECT_EQ(empty.patch_count, 0u);
}

TEST(PatchList, PatchListRemoveFreesASlotForTheNextAdd)
{
    RoomScene scene = scene_with(static_cast<std::uint32_t>(RoomScene::K_MAX_PATCHES));
    ui::remove_patch(scene, 0);
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES - 1);
    EXPECT_TRUE(ui::add_patch(scene, Surface::Floor, MaterialId::OpenDoorway, 2.0));
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);
}

TEST(PatchList, PatchListSetChangesTheAreaOfOneEntry)
{
    RoomScene scene = scene_with(3);
    EXPECT_TRUE(ui::set_patch_area(scene, 1, 7.25));

    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 1.0);
    EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 7.25);
    EXPECT_DOUBLE_EQ(scene.patches[2].area_m2, 3.0);
    EXPECT_EQ(scene.patch_count, 3u);
}

TEST(PatchList, PatchListSetRefusesABadIndexOrABadArea)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::set_patch_area(scene, 2, 5.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 15, 5.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, 0.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, -2.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, std::numeric_limits<double>::infinity()));

    EXPECT_EQ(scene.patch_count, 2u);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 1.0);
    EXPECT_DOUBLE_EQ(scene.patches[1].area_m2, 2.0);
    EXPECT_DOUBLE_EQ(scene.patches[2].area_m2, 0.0);
}

TEST(PatchList, PatchListDefaultAreaIsATenthOfTheDefaultFace)
{
    const RoomScene defaults;
    for (std::size_t face = 0; face < SushiDSP::room::K_SURFACE_COUNT; ++face)
    {
        const Surface surface = static_cast<Surface>(face);
        EXPECT_DOUBLE_EQ(ui::default_patch_area(surface), 0.1 * defaults.face_area_m2(surface));
        EXPECT_GT(ui::default_patch_area(surface), 0.0);
    }
}

TEST(PatchList, PatchListAddAcceptsAnAreaLargerThanItsFaceAndTheSceneRefusesIt)
{
    RoomScene scene;
    const double too_large = scene.face_area_m2(Surface::PlusX) + 1.0;
    ASSERT_TRUE(ui::add_patch(scene, Surface::PlusX, MaterialId::OpenDoorway, too_large));
    EXPECT_EQ(scene.patch_count, 1u);

    std::string reason;
    EXPECT_FALSE(scene.is_valid(reason));
    EXPECT_NE(reason.find("patch"), std::string::npos) << reason;
}
