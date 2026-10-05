/****************************************************************/
/* test_furnishing_list.cpp                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_furnishing_list.cpp
 * @brief Tests for the pure furnishing-list edits the room panel calls.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "SushiDSP/room/Furniture.hpp"
#include "SushiDSP/room/RoomScene.hpp"

#include "ui/FurnishingList.hpp"

namespace
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::RoomScene;

    /// @brief Builds a scene holding count entries whose quantities read 1, 2, 3 and so on.
    RoomScene scene_with(std::uint32_t count)
    {
        RoomScene scene;
        for (std::uint32_t index = 0; index < count; ++index)
        {
            EXPECT_TRUE(ui::add_furnishing(scene, FurnitureId::SofaTwoSeat,
                                           static_cast<double>(index + 1)));
        }
        return scene;
    }
}

TEST(Unit_Dsp, FurnishingListAddFillsToTheLimitAndRefusesTheNext)
{
    RoomScene scene;
    for (std::size_t index = 0; index < RoomScene::kMaxFurnishings; ++index)
    {
        ASSERT_TRUE(ui::add_furnishing(scene, FurnitureId::SofaTwoSeat,
                                       static_cast<double>(index + 1)));
        EXPECT_EQ(scene.furnishing_count, index + 1);
    }
    EXPECT_EQ(scene.furnishing_count, RoomScene::kMaxFurnishings);

    EXPECT_FALSE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 9.0));
    EXPECT_EQ(scene.furnishing_count, RoomScene::kMaxFurnishings);
    for (std::size_t index = 0; index < RoomScene::kMaxFurnishings; ++index)
    {
        EXPECT_EQ(scene.furnishings[index].item, FurnitureId::SofaTwoSeat);
        EXPECT_DOUBLE_EQ(scene.furnishings[index].quantity, static_cast<double>(index + 1));
    }
}

TEST(Unit_Dsp, FurnishingListAddRecordsTheItemAndTheQuantity)
{
    RoomScene scene;
    ASSERT_TRUE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 3.5));
    ASSERT_EQ(scene.furnishing_count, 1u);
    EXPECT_EQ(scene.furnishings[0].item, FurnitureId::WardrobeModule);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 3.5);
}

TEST(Unit_Dsp, FurnishingListAddRefusesZeroNegativeNaNAndInfinity)
{
    const double refused[] = {
        0.0,
        -0.0,
        -1.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    RoomScene scene = scene_with(2);
    for (double quantity : refused)
    {
        EXPECT_FALSE(ui::add_furnishing(scene, FurnitureId::SofaTwoSeat, quantity));
        EXPECT_EQ(scene.furnishing_count, 2u);
        EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 1.0);
        EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 2.0);
        EXPECT_DOUBLE_EQ(scene.furnishings[2].quantity, 0.0);
    }
}

TEST(Unit_Dsp, FurnishingListRemoveShiftsTheTailDownAndKeepsTheOrder)
{
    RoomScene scene = scene_with(4);
    ui::remove_furnishing(scene, 1);

    ASSERT_EQ(scene.furnishing_count, 3u);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 3.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[2].quantity, 4.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[3].quantity, 0.0);

    ui::remove_furnishing(scene, 2);
    ASSERT_EQ(scene.furnishing_count, 2u);
    EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 3.0);

    ui::remove_furnishing(scene, 0);
    ASSERT_EQ(scene.furnishing_count, 1u);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 3.0);
}

TEST(Unit_Dsp, FurnishingListRemoveIgnoresAnIndexAtOrPastTheCount)
{
    RoomScene scene = scene_with(3);
    ui::remove_furnishing(scene, 3);
    ui::remove_furnishing(scene, 4);
    ui::remove_furnishing(scene, std::numeric_limits<std::uint32_t>::max());

    ASSERT_EQ(scene.furnishing_count, 3u);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 2.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[2].quantity, 3.0);

    RoomScene empty;
    ui::remove_furnishing(empty, 0);
    EXPECT_EQ(empty.furnishing_count, 0u);
}

TEST(Unit_Dsp, FurnishingListRemoveFreesASlotForTheNextAdd)
{
    RoomScene scene = scene_with(static_cast<std::uint32_t>(RoomScene::kMaxFurnishings));
    ui::remove_furnishing(scene, 0);
    EXPECT_EQ(scene.furnishing_count, RoomScene::kMaxFurnishings - 1);
    EXPECT_TRUE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 2.0));
    EXPECT_EQ(scene.furnishing_count, RoomScene::kMaxFurnishings);
}

TEST(Unit_Dsp, FurnishingListSetChangesTheQuantityOfOneEntry)
{
    RoomScene scene = scene_with(3);
    EXPECT_TRUE(ui::set_furnishing_quantity(scene, 1, 7.25));

    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 7.25);
    EXPECT_DOUBLE_EQ(scene.furnishings[2].quantity, 3.0);
    EXPECT_EQ(scene.furnishing_count, 3u);
}

TEST(Unit_Dsp, FurnishingListSetRefusesABadIndexOrABadQuantity)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 2, 5.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 15, 5.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, 0.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, -2.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, std::numeric_limits<double>::infinity()));

    EXPECT_EQ(scene.furnishing_count, 2u);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[1].quantity, 2.0);
    EXPECT_DOUBLE_EQ(scene.furnishings[2].quantity, 0.0);
}

TEST(Unit_Dsp, FurnishingListDefaultsToOneItemForEveryRow)
{
    for (std::size_t index = 0; index < SushiDSP::room::kFurnitureCount; ++index)
    {
        EXPECT_DOUBLE_EQ(ui::default_furnishing_quantity(static_cast<FurnitureId>(index)), 1.0);
    }
}
