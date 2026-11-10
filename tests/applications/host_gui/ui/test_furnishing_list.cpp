/**************************************************************************/
/* test_furnishing_list.cpp                                               */
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
 * @file test_furnishing_list.cpp
 * @brief Tests for the pure furnishing-list edits the room panel calls.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "ui/furnishing_list.hpp"

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

    /// @brief Adds entries to a scene until the list refuses one.
    /// @param scene The scene to fill.
    /// @return How many adds the list accepted.
    std::size_t fill_until_refused(RoomScene& scene)
    {
        std::size_t accepted = 0;
        while (ui::add_furnishing(scene, FurnitureId::SofaTwoSeat, static_cast<double>(accepted + 1)))
        {
            ++accepted;
        }
        return accepted;
    }

    /// @brief Reads the quantities of the first count entries of a scene.
    std::vector<double> quantities_of(const RoomScene& scene, std::size_t count)
    {
        std::vector<double> quantities;
        for (std::size_t index = 0; index < count; ++index)
        {
            quantities.push_back(scene.furnishings[index].quantity);
        }
        return quantities;
    }

    /// @brief The quantities 1, 2, 3 and so on, count of them.
    std::vector<double> ramp(std::size_t count)
    {
        std::vector<double> quantities;
        for (std::size_t index = 0; index < count; ++index)
        {
            quantities.push_back(static_cast<double>(index + 1));
        }
        return quantities;
    }

    /// @brief Counts the entries of a scene that are not a two-seat sofa.
    std::size_t entries_other_than_sofas(const RoomScene& scene)
    {
        std::size_t others = 0;
        for (std::size_t index = 0; index < scene.furnishing_count; ++index)
        {
            others += scene.furnishings[index].item == FurnitureId::SofaTwoSeat ? 0u : 1u;
        }
        return others;
    }

    /// @brief One quantity the list refuses.
    struct RefusedQuantityRow
    {
            const char* name;
            double quantity;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string row_name(const ::testing::TestParamInfo<RefusedQuantityRow>& info)
    {
        return std::string(info.param.name);
    }
} // namespace

TEST(Small_FurnishingList, FurnishingListAddFillsToTheLimit)
{
    RoomScene scene;

    EXPECT_EQ(fill_until_refused(scene), RoomScene::K_MAX_FURNISHINGS);
    EXPECT_EQ(scene.furnishing_count, RoomScene::K_MAX_FURNISHINGS);
}

TEST(Small_FurnishingList, FurnishingListAddKeepsEveryFilledEntryInItsSlot)
{
    RoomScene scene;
    static_cast<void>(fill_until_refused(scene));

    EXPECT_EQ(quantities_of(scene, RoomScene::K_MAX_FURNISHINGS), ramp(RoomScene::K_MAX_FURNISHINGS));
    EXPECT_EQ(entries_other_than_sofas(scene), 0u);
}

TEST(Small_FurnishingList, FurnishingListAddRefusesTheNextEntryOnceFull)
{
    RoomScene scene;
    static_cast<void>(fill_until_refused(scene));

    EXPECT_FALSE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 9.0));
    EXPECT_EQ(scene.furnishing_count, RoomScene::K_MAX_FURNISHINGS);
}

TEST(Small_FurnishingList, FurnishingListAddRecordsTheItemAndTheQuantity)
{
    RoomScene scene;
    ASSERT_TRUE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 3.5));
    ASSERT_EQ(scene.furnishing_count, 1u);
    EXPECT_EQ(scene.furnishings[0].item, FurnitureId::WardrobeModule);
    EXPECT_DOUBLE_EQ(scene.furnishings[0].quantity, 3.5);
}

class Small_FurnishingListRefusedQuantity : public ::testing::TestWithParam<RefusedQuantityRow>
{
};

TEST_P(Small_FurnishingListRefusedQuantity, FurnishingListAddRefusesTheQuantityAndChangesNothing)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::add_furnishing(scene, FurnitureId::SofaTwoSeat, GetParam().quantity));
    EXPECT_EQ(scene.furnishing_count, 2u);
    EXPECT_EQ(quantities_of(scene, 3), (std::vector<double>{1.0, 2.0, 0.0}));
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_FurnishingListRefusedQuantity,
    ::testing::Values(RefusedQuantityRow{"Zero", 0.0}, RefusedQuantityRow{"NegativeZero", -0.0},
                      RefusedQuantityRow{"Negative", -1.0},
                      RefusedQuantityRow{"NaN", std::numeric_limits<double>::quiet_NaN()},
                      RefusedQuantityRow{"Infinity", std::numeric_limits<double>::infinity()},
                      RefusedQuantityRow{"NegativeInfinity", -std::numeric_limits<double>::infinity()}),
    row_name);

TEST(Small_FurnishingList, FurnishingListRemoveShiftsTheTailDown)
{
    RoomScene scene = scene_with(4);
    ui::remove_furnishing(scene, 1);

    ASSERT_EQ(scene.furnishing_count, 3u);
    EXPECT_EQ(quantities_of(scene, 4), (std::vector<double>{1.0, 3.0, 4.0, 0.0}));
}

TEST(Small_FurnishingList, FurnishingListRemoveKeepsTheOrderOfTheRest)
{
    RoomScene scene = scene_with(4);
    ui::remove_furnishing(scene, 1);
    ui::remove_furnishing(scene, 2);

    ASSERT_EQ(scene.furnishing_count, 2u);
    EXPECT_EQ(quantities_of(scene, 2), (std::vector<double>{1.0, 3.0}));
}

TEST(Small_FurnishingList, FurnishingListRemoveTakesTheFirstEntryAway)
{
    RoomScene scene = scene_with(2);
    ui::remove_furnishing(scene, 0);

    ASSERT_EQ(scene.furnishing_count, 1u);
    EXPECT_EQ(quantities_of(scene, 1), (std::vector<double>{2.0}));
}

TEST(Small_FurnishingList, FurnishingListRemoveIgnoresAnIndexAtOrPastTheCount)
{
    RoomScene scene = scene_with(3);
    ui::remove_furnishing(scene, 3);
    ui::remove_furnishing(scene, 4);
    ui::remove_furnishing(scene, std::numeric_limits<std::uint32_t>::max());

    ASSERT_EQ(scene.furnishing_count, 3u);
    EXPECT_EQ(quantities_of(scene, 3), (std::vector<double>{1.0, 2.0, 3.0}));

    RoomScene empty;
    ui::remove_furnishing(empty, 0);
    EXPECT_EQ(empty.furnishing_count, 0u);
}

TEST(Small_FurnishingList, FurnishingListRemoveFreesASlotForTheNextAdd)
{
    RoomScene scene = scene_with(static_cast<std::uint32_t>(RoomScene::K_MAX_FURNISHINGS));
    ui::remove_furnishing(scene, 0);
    EXPECT_EQ(scene.furnishing_count, RoomScene::K_MAX_FURNISHINGS - 1);
    EXPECT_TRUE(ui::add_furnishing(scene, FurnitureId::WardrobeModule, 2.0));
    EXPECT_EQ(scene.furnishing_count, RoomScene::K_MAX_FURNISHINGS);
}

TEST(Small_FurnishingList, FurnishingListSetChangesTheQuantityOfOneEntry)
{
    RoomScene scene = scene_with(3);
    EXPECT_TRUE(ui::set_furnishing_quantity(scene, 1, 7.25));

    EXPECT_EQ(quantities_of(scene, 3), (std::vector<double>{1.0, 7.25, 3.0}));
    EXPECT_EQ(scene.furnishing_count, 3u);
}

TEST(Small_FurnishingList, FurnishingListSetRefusesABadIndexOrABadQuantity)
{
    RoomScene scene = scene_with(2);

    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 2, 5.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 15, 5.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, 0.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, -2.0));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(ui::set_furnishing_quantity(scene, 0, std::numeric_limits<double>::infinity()));

    EXPECT_EQ(scene.furnishing_count, 2u);
    EXPECT_EQ(quantities_of(scene, 3), (std::vector<double>{1.0, 2.0, 0.0}));
}

TEST(Small_FurnishingList, FurnishingListPlacesAnEntryInsideTheRoom)
{
    RoomScene scene;
    scene.furnishing_count = 0;
    ASSERT_TRUE(ui::add_furnishing(scene, FurnitureId::Desk, 1.0));
    EXPECT_FALSE(scene.furnishings[0].placed);

    ASSERT_TRUE(ui::place_furnishing(scene, 0, 100.0, 1.5));
    EXPECT_TRUE(scene.furnishings[0].placed);
    EXPECT_EQ(scene.furnishings[0].x_m, scene.dimensions.x);
    EXPECT_EQ(scene.furnishings[0].y_m, 1.5);
    EXPECT_FALSE(ui::place_furnishing(scene, 1, 1.0, 1.0));
}

TEST(Small_FurnishingList, FurnishingListTakesAnEntryItsPlaceAway)
{
    RoomScene scene;
    scene.furnishing_count = 0;
    ASSERT_TRUE(ui::add_furnishing(scene, FurnitureId::Desk, 1.0));
    ASSERT_TRUE(ui::place_furnishing(scene, 0, 1.0, 1.5));

    ASSERT_TRUE(ui::unplace_furnishing(scene, 0));
    EXPECT_FALSE(scene.furnishings[0].placed);
    EXPECT_EQ(scene.furnishings[0].x_m, 0.0);
    EXPECT_FALSE(ui::unplace_furnishing(scene, 1));
}
