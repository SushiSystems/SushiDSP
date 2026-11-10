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
 * @file test_patch_list.cpp
 * @brief Tests for the pure patch-list edits the room panel calls.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

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

    /// @brief Adds patches to a scene until the list refuses one.
    /// @param scene The scene to fill.
    /// @return How many adds the list accepted.
    std::size_t fill_until_refused(RoomScene& scene)
    {
        std::size_t accepted = 0;
        while (ui::add_patch(scene, Surface::PlusX, MaterialId::GlassWindow, static_cast<double>(accepted + 1)))
        {
            ++accepted;
        }
        return accepted;
    }

    /// @brief Reads the areas of the first count patches of a scene.
    std::vector<double> areas_of(const RoomScene& scene, std::size_t count)
    {
        std::vector<double> areas;
        for (std::size_t index = 0; index < count; ++index)
        {
            areas.push_back(scene.patches[index].area_m2);
        }
        return areas;
    }

    /// @brief The areas 1, 2, 3 and so on, count of them.
    std::vector<double> ramp(std::size_t count)
    {
        std::vector<double> areas;
        for (std::size_t index = 0; index < count; ++index)
        {
            areas.push_back(static_cast<double>(index + 1));
        }
        return areas;
    }

    /// @brief Counts the patches of a scene that are not on the +X face in glass.
    std::size_t patches_other_than_glass_on_plus_x(const RoomScene& scene)
    {
        std::size_t others = 0;
        for (std::size_t index = 0; index < scene.patch_count; ++index)
        {
            const bool expected = scene.patches[index].surface == Surface::PlusX &&
                                  scene.patches[index].material == MaterialId::GlassWindow;
            others += expected ? 0u : 1u;
        }
        return others;
    }

    /// @brief One area the list refuses.
    struct RefusedAreaRow
    {
            const char* name;
            double area;
    };

    /// @brief One face of the room.
    struct SurfaceRow
    {
            const char* name;
            Surface surface;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    template <typename Row>
    std::string row_name(const ::testing::TestParamInfo<Row>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

TEST(Small_PatchList, PatchListAddFillsToTheLimit)
{
    RoomScene scene;

    EXPECT_EQ(fill_until_refused(scene), RoomScene::K_MAX_PATCHES);
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);
}

TEST(Small_PatchList, PatchListAddKeepsEveryFilledPatchInItsSlot)
{
    RoomScene scene;
    static_cast<void>(fill_until_refused(scene));

    EXPECT_EQ(areas_of(scene, RoomScene::K_MAX_PATCHES), ramp(RoomScene::K_MAX_PATCHES));
    EXPECT_EQ(patches_other_than_glass_on_plus_x(scene), 0u);
}

TEST(Small_PatchList, PatchListAddRefusesTheNextPatchOnceFull)
{
    RoomScene scene;
    static_cast<void>(fill_until_refused(scene));

    EXPECT_FALSE(ui::add_patch(scene, Surface::Floor, MaterialId::OpenDoorway, 9.0));
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);
}

TEST(Small_PatchList, PatchListAddRecordsTheFaceTheMaterialAndTheArea)
{
    RoomScene scene;
    ASSERT_TRUE(ui::add_patch(scene, Surface::Ceiling, MaterialId::OpenDoorway, 3.5));
    ASSERT_EQ(scene.patch_count, 1u);
    EXPECT_EQ(scene.patches[0].surface, Surface::Ceiling);
    EXPECT_EQ(scene.patches[0].material, MaterialId::OpenDoorway);
    EXPECT_DOUBLE_EQ(scene.patches[0].area_m2, 3.5);
}

class Small_PatchListRefusedArea : public ::testing::TestWithParam<RefusedAreaRow>
{
};

TEST_P(Small_PatchListRefusedArea, PatchListAddRefusesTheAreaAndChangesNothing)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::add_patch(scene, Surface::PlusX, MaterialId::GlassWindow, GetParam().area));
    EXPECT_EQ(scene.patch_count, 2u);
    EXPECT_EQ(areas_of(scene, 3), (std::vector<double>{1.0, 2.0, 0.0}));
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_PatchListRefusedArea,
    ::testing::Values(RefusedAreaRow{"Zero", 0.0}, RefusedAreaRow{"NegativeZero", -0.0},
                      RefusedAreaRow{"Negative", -1.0},
                      RefusedAreaRow{"NaN", std::numeric_limits<double>::quiet_NaN()},
                      RefusedAreaRow{"Infinity", std::numeric_limits<double>::infinity()},
                      RefusedAreaRow{"NegativeInfinity", -std::numeric_limits<double>::infinity()}),
    row_name<RefusedAreaRow>);

TEST(Small_PatchList, PatchListRemoveShiftsTheTailDown)
{
    RoomScene scene = scene_with(4);
    ui::remove_patch(scene, 1);

    ASSERT_EQ(scene.patch_count, 3u);
    EXPECT_EQ(areas_of(scene, 4), (std::vector<double>{1.0, 3.0, 4.0, 0.0}));
}

TEST(Small_PatchList, PatchListRemoveClearsTheSlotTheTailLeft)
{
    RoomScene scene = scene_with(4);
    ui::remove_patch(scene, 1);

    EXPECT_EQ(scene.patches[3].surface, Surface::MinusX);
    EXPECT_EQ(scene.patches[3].material, static_cast<MaterialId>(0));
}

TEST(Small_PatchList, PatchListRemoveKeepsTheOrderOfTheRest)
{
    RoomScene scene = scene_with(4);
    ui::remove_patch(scene, 1);
    ui::remove_patch(scene, 2);

    ASSERT_EQ(scene.patch_count, 2u);
    EXPECT_EQ(areas_of(scene, 2), (std::vector<double>{1.0, 3.0}));
}

TEST(Small_PatchList, PatchListRemoveTakesTheFirstPatchAway)
{
    RoomScene scene = scene_with(2);
    ui::remove_patch(scene, 0);

    ASSERT_EQ(scene.patch_count, 1u);
    EXPECT_EQ(areas_of(scene, 1), (std::vector<double>{2.0}));
}

TEST(Small_PatchList, PatchListRemoveIgnoresAnIndexAtOrPastTheCount)
{
    RoomScene scene = scene_with(3);
    ui::remove_patch(scene, 3);
    ui::remove_patch(scene, 4);
    ui::remove_patch(scene, std::numeric_limits<std::uint32_t>::max());

    ASSERT_EQ(scene.patch_count, 3u);
    EXPECT_EQ(areas_of(scene, 3), (std::vector<double>{1.0, 2.0, 3.0}));

    RoomScene empty;
    ui::remove_patch(empty, 0);
    EXPECT_EQ(empty.patch_count, 0u);
}

TEST(Small_PatchList, PatchListRemoveFreesASlotForTheNextAdd)
{
    RoomScene scene = scene_with(static_cast<std::uint32_t>(RoomScene::K_MAX_PATCHES));
    ui::remove_patch(scene, 0);
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES - 1);
    EXPECT_TRUE(ui::add_patch(scene, Surface::Floor, MaterialId::OpenDoorway, 2.0));
    EXPECT_EQ(scene.patch_count, RoomScene::K_MAX_PATCHES);
}

TEST(Small_PatchList, PatchListSetChangesTheAreaOfOneEntry)
{
    RoomScene scene = scene_with(3);
    EXPECT_TRUE(ui::set_patch_area(scene, 1, 7.25));

    EXPECT_EQ(areas_of(scene, 3), (std::vector<double>{1.0, 7.25, 3.0}));
    EXPECT_EQ(scene.patch_count, 3u);
}

TEST(Small_PatchList, PatchListSetRefusesABadIndexOrABadArea)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::set_patch_area(scene, 2, 5.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 15, 5.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, 0.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, -2.0));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(ui::set_patch_area(scene, 0, std::numeric_limits<double>::infinity()));

    EXPECT_EQ(scene.patch_count, 2u);
    EXPECT_EQ(areas_of(scene, 3), (std::vector<double>{1.0, 2.0, 0.0}));
}

class Small_PatchListSurface : public ::testing::TestWithParam<SurfaceRow>
{
};

TEST_P(Small_PatchListSurface, PatchListDefaultAreaIsATenthOfTheDefaultFace)
{
    const RoomScene defaults;
    const Surface surface = GetParam().surface;

    EXPECT_DOUBLE_EQ(ui::default_patch_area(surface), 0.1 * defaults.face_area_m2(surface));
    EXPECT_GT(ui::default_patch_area(surface), 0.0);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_PatchListSurface,
                         ::testing::Values(SurfaceRow{"MinusX", Surface::MinusX},
                                           SurfaceRow{"PlusX", Surface::PlusX},
                                           SurfaceRow{"MinusY", Surface::MinusY},
                                           SurfaceRow{"PlusY", Surface::PlusY},
                                           SurfaceRow{"Floor", Surface::Floor},
                                           SurfaceRow{"Ceiling", Surface::Ceiling}),
                         row_name<SurfaceRow>);

TEST(Small_PatchList, PatchListAddAcceptsAnAreaLargerThanItsFace)
{
    RoomScene scene;
    const double too_large = scene.face_area_m2(Surface::PlusX) + 1.0;

    EXPECT_TRUE(ui::add_patch(scene, Surface::PlusX, MaterialId::OpenDoorway, too_large));
    EXPECT_EQ(scene.patch_count, 1u);
}

TEST(Small_PatchList, PatchListSceneRefusesAPatchLargerThanItsFace)
{
    RoomScene scene;
    const double too_large = scene.face_area_m2(Surface::PlusX) + 1.0;
    ASSERT_TRUE(ui::add_patch(scene, Surface::PlusX, MaterialId::OpenDoorway, too_large));

    std::string reason;
    EXPECT_FALSE(scene.is_valid(reason));
    EXPECT_NE(reason.find("patch"), std::string::npos) << reason;
}
