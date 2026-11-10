/**************************************************************************/
/* test_furnishings.cpp                                                   */
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
 * @file test_furnishings.cpp
 * @brief Tests for the furnishing seam of RoomScene: its area, its validity checks, its decay
 * bound and the entries past the count.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include <SushiDSP/nodes/room/band_response.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/medium_absorption.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

#include <support/tolerance.hpp>

namespace
{
    using SushiDSP::room::FurnitureId;
    using SushiDSP::room::K_BAND_COUNT;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;

    /// @brief Holds one value per octave band, 125 Hz to 4 kHz.
    using Bands = std::array<double, K_BAND_COUNT>;

    /// @brief Counts the rounded operations of a row divided by four volumes: the product 4 V is
    /// exact and the division rounds once, with a margin.
    constexpr std::size_t K_COEFFICIENT_OPERATIONS = 4;

    /// @brief Names the item a stale furnishing carries: a row no table holds.
    constexpr FurnitureId K_UNKNOWN_ITEM = static_cast<FurnitureId>(200);

    /// @brief Returns a furniture row's absorption area, in m2 sabine, one value per band.
    Bands row_area(FurnitureId item)
    {
        Bands area{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            area[band] = static_cast<double>(SushiDSP::room::furniture(item).absorption_area[band]);
        }
        return area;
    }

    /// @brief Returns a row scaled by a quantity. A float row times a small whole number is exact
    /// in double.
    Bands scaled(const Bands& row, double quantity)
    {
        Bands result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            result[band] = quantity * row[band];
        }
        return result;
    }

    /// @brief Returns the sum of two arrays, band by band; exact for the whole-number
    /// quantities of the rows below.
    Bands sum_of(const Bands& first, const Bands& second)
    {
        Bands result{};
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            result[band] = first[band] + second[band];
        }
        return result;
    }

    /// @brief Checks each band's furnishing coefficient against one sofa's row over four volumes.
    void expect_sofa_coefficients(const SushiDSP::room::MediumAbsorption& medium, double four_volumes)
    {
        const Bands sofa = row_area(FurnitureId::SofaTwoSeat);
        for (std::size_t band = 0; band < K_BAND_COUNT; ++band)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
                medium.furnishing_coefficient_per_metre(band), sofa[band] / four_volumes,
                K_COEFFICIENT_OPERATIONS)))
                << "band " << band;
        }
    }

    /// @brief Builds the garage holding one entry of the given item and quantity.
    RoomScene garage_with(FurnitureId item, double quantity)
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        scene.furnishings[0] = {item, quantity};
        scene.furnishing_count = 1;
        return scene;
    }

    /// @brief Builds the garage holding the given number of live entries, each a small bed.
    RoomScene garage_with_beds(std::size_t count)
    {
        RoomScene scene = SushiDSP::test::garage_scene();
        for (RoomScene::Furnishing& entry : scene.furnishings)
        {
            entry = {FurnitureId::BedDouble, 0.05};
        }
        scene.furnishing_count = static_cast<std::uint32_t>(count);
        return scene;
    }

    /// @brief Fills every entry of a scene with a row no table holds and sets the count to zero.
    RoomScene with_stale_entries(RoomScene scene)
    {
        for (RoomScene::Furnishing& entry : scene.furnishings)
        {
            entry = {K_UNKNOWN_ITEM, -1.0};
        }
        scene.furnishing_count = 0;
        return scene;
    }

    /// @brief Builds the 12 m cube of unpainted concrete, whose bare surfaces sit under the
    /// bound, holding the given quantity of two-seat sofas; none when the quantity is zero.
    RoomScene concrete_cube_with_sofas(double quantity)
    {
        RoomScene cube;
        cube.dimensions = {12.0, 12.0, 12.0};
        cube.surfaces.fill(MaterialId::ConcreteUnpainted);
        cube.furnishings[0] = {FurnitureId::SofaTwoSeat, quantity};
        cube.furnishing_count = quantity > 0.0 ? 1u : 0u;
        return cube;
    }

    /// @brief Holds a sofa quantity and whether the 12 m concrete cube holding it passes the bound.
    struct SofaQuantity
    {
            const char* name;
            double quantity;
            bool valid;
    };

    /// @brief Holds a quantity between the linear form's 5.475 and the exact form's 5.577 sofas,
    /// and one past both; see tests/nodes/room/test_room_scene/README.md.
    const std::array<SofaQuantity, 2> K_SOFA_QUANTITIES = {
        {{"BetweenTheLinearAndTheExactForm", 5.52, false}, {"PastTheExactForm", 5.63, true}}};

    /// @brief Names a sofa quantity case.
    std::string sofa_quantity_name(const ::testing::TestParamInfo<SofaQuantity>& info)
    {
        return info.param.name;
    }

    /// @brief Holds a quantity a furnishing must be refused for.
    struct BadQuantity
    {
            const char* name;
            double quantity;
    };

    /// @brief Holds the four quantities that are not a positive finite number.
    const std::array<BadQuantity, 4> K_BAD_QUANTITIES = {
        {{"Zero", 0.0},
         {"Negative", -1.0},
         {"NotANumber", std::numeric_limits<double>::quiet_NaN()},
         {"Infinity", std::numeric_limits<double>::infinity()}}};

    /// @brief Names a bad quantity case.
    std::string bad_quantity_name(const ::testing::TestParamInfo<BadQuantity>& info)
    {
        return info.param.name;
    }

    /// @brief Holds a reference room, to keep it valid when unfurnished.
    struct NamedRoom
    {
            const char* name;
            RoomScene (*build)();
    };

    /// @brief Holds the three reference rooms.
    const std::array<NamedRoom, 3> K_ROOMS = {{{"Garage", SushiDSP::test::garage_scene},
                                               {"Bedroom", SushiDSP::test::bedroom_scene},
                                               {"BlockRoom", SushiDSP::test::block_room_scene}}};

    /// @brief Names a room case.
    std::string room_name(const ::testing::TestParamInfo<NamedRoom>& info)
    {
        return info.param.name;
    }

    /// @brief Holds the place and yaw a placed furnishing is refused at.
    struct BadPlace
    {
            const char* name;
            double x_m;
            double y_m;
            double yaw_rad;
    };

    /// @brief Holds a place past each wall of the 6 x 4 m garage and a yaw that is not a number.
    const std::array<BadPlace, 5> K_BAD_PLACES = {
        {{"BehindTheMinusXWall", -0.1, 1.0, 0.0},
         {"PastThePlusXWall", 6.1, 1.0, 0.0},
         {"BehindTheMinusYWall", 1.0, -0.1, 0.0},
         {"PastThePlusYWall", 1.0, 4.1, 0.0},
         {"AYawThatIsNotANumber", 1.0, 1.0, std::numeric_limits<double>::quiet_NaN()}}};

    /// @brief Names a bad place case.
    std::string bad_place_name(const ::testing::TestParamInfo<BadPlace>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_RoomSceneSofaQuantity : public ::testing::TestWithParam<SofaQuantity>
{
};

class Small_RoomSceneBadQuantity : public ::testing::TestWithParam<BadQuantity>
{
};

class Small_RoomSceneUnfurnishedRoom : public ::testing::TestWithParam<NamedRoom>
{
};

class Small_RoomSceneBadPlace : public ::testing::TestWithParam<BadPlace>
{
};

TEST(Small_RoomScene, RoomSceneSumsItsFurnishingAreaByQuantity)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishings[1] = {FurnitureId::SofaTwoSeat, 2.0};
    scene.furnishing_count = 2;

    EXPECT_EQ(scene.furnishing_absorption_area(),
              sum_of(scaled(row_area(FurnitureId::WardrobeModule), 3.0),
                     scaled(row_area(FurnitureId::SofaTwoSeat), 2.0)));
}

TEST(Small_RoomScene, RoomSceneWithoutFurnishingsHasNoFurnishingArea)
{
    EXPECT_EQ(SushiDSP::test::garage_scene().furnishing_absorption_area(), Bands{});
}

TEST(Small_RoomScene, RoomSceneKeepsItsMeanAbsorptionWhenFurnished)
{
    const RoomScene bare = SushiDSP::test::bedroom_scene();
    RoomScene furnished = bare;
    furnished.furnishings[0] = {FurnitureId::SofaTwoSeat, 2.0};
    furnished.furnishing_count = 1;

    EXPECT_EQ(furnished.mean_absorption(), bare.mean_absorption());
}

TEST_P(Small_RoomSceneBadQuantity, RoomSceneRefusesAFurnishingWhoseQuantityIsNotPositiveAndFinite)
{
    std::string reason;
    EXPECT_FALSE(garage_with(FurnitureId::SofaTwoSeat, GetParam().quantity).is_valid(reason));
    EXPECT_NE(reason.find("non-positive quantity"), std::string::npos) << reason;
}

INSTANTIATE_TEST_SUITE_P(Quantities, Small_RoomSceneBadQuantity, ::testing::ValuesIn(K_BAD_QUANTITIES),
                         bad_quantity_name);

TEST(Small_RoomScene, RoomSceneRefusesAFurnishingThatNamesNoRow)
{
    std::string reason;
    EXPECT_FALSE(garage_with(K_UNKNOWN_ITEM, 1.0).is_valid(reason));
    EXPECT_NE(reason.find("names no furniture row"), std::string::npos) << reason;
}

TEST(Small_RoomScene, RoomSceneRefusesMoreFurnishingsThanItHolds)
{
    std::string reason;
    EXPECT_FALSE(garage_with_beds(RoomScene::K_MAX_FURNISHINGS + 1).is_valid(reason));
    EXPECT_NE(reason.find("16"), std::string::npos) << reason;
}

TEST(Small_RoomScene, RoomSceneAcceptsExactlyTheMaximumFurnishings)
{
    std::string reason;
    EXPECT_TRUE(garage_with_beds(RoomScene::K_MAX_FURNISHINGS).is_valid(reason)) << reason;
}

TEST(Small_RoomScene, RoomSceneRefusesABadFurnishingAfterAGoodOne)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::SofaTwoSeat, 1.0};
    scene.furnishings[1] = {FurnitureId::BedDouble, 0.0};
    scene.furnishing_count = 2;
    std::string reason;
    EXPECT_FALSE(scene.is_valid(reason));
    EXPECT_NE(reason.find("furnishing 1 "), std::string::npos) << reason;
}

TEST_P(Small_RoomSceneBadPlace, RoomSceneRefusesAPlacedFurnishingThatStandsOutsideTheRoom)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::SofaTwoSeat, 1.0, true, GetParam().x_m, GetParam().y_m,
                            GetParam().yaw_rad};
    scene.furnishing_count = 1;
    std::string reason;
    EXPECT_FALSE(scene.is_valid(reason));
    EXPECT_NE(reason.find("stands outside the room"), std::string::npos) << reason;
}

INSTANTIATE_TEST_SUITE_P(Places, Small_RoomSceneBadPlace, ::testing::ValuesIn(K_BAD_PLACES), bad_place_name);

TEST(Small_RoomScene, RoomSceneIgnoresThePlaceOfAnUnplacedFurnishing)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::SofaTwoSeat, 1.0, false, 99.0, -99.0, 0.0};
    scene.furnishing_count = 1;
    std::string reason;
    EXPECT_TRUE(scene.is_valid(reason)) << reason;
}

TEST(Small_RoomScene, RoomSceneCountsFurnitureTowardTheDecayBound)
{
    std::string reason;
    EXPECT_FALSE(concrete_cube_with_sofas(0.0).is_valid(reason));
    EXPECT_TRUE(concrete_cube_with_sofas(6.0).is_valid(reason)) << reason;
}

TEST_P(Small_RoomSceneSofaQuantity, RoomSceneBoundUsesTheExactFormNotTheLinearOne)
{
    std::string reason;
    EXPECT_EQ(concrete_cube_with_sofas(GetParam().quantity).is_valid(reason), GetParam().valid) << reason;
}

INSTANTIATE_TEST_SUITE_P(Quantities, Small_RoomSceneSofaQuantity, ::testing::ValuesIn(K_SOFA_QUANTITIES),
                         sofa_quantity_name);

TEST_P(Small_RoomSceneUnfurnishedRoom, RoomSceneStaysValidUnfurnishedWhateverStaleEntriesItHolds)
{
    std::string reason;
    EXPECT_TRUE(with_stale_entries(GetParam().build()).is_valid(reason)) << reason;
}

INSTANTIATE_TEST_SUITE_P(Rooms, Small_RoomSceneUnfurnishedRoom, ::testing::ValuesIn(K_ROOMS), room_name);

TEST(Small_RoomScene, FurnishingAreaIgnoresEntriesBeyondTheCount)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::SofaTwoSeat, 1.0};
    scene.furnishings[1] = {FurnitureId::BedDouble, 1.0};
    scene.furnishing_count = 1;

    EXPECT_EQ(scene.furnishing_absorption_area(), row_area(FurnitureId::SofaTwoSeat));
}

TEST(Small_RoomScene, FurnishingAreaReadsAtMostTheMaximumEntries)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    for (RoomScene::Furnishing& entry : scene.furnishings)
    {
        entry = {FurnitureId::SofaTwoSeat, 1.0};
    }
    scene.furnishing_count = static_cast<std::uint32_t>(RoomScene::K_MAX_FURNISHINGS + 5);

    EXPECT_EQ(scene.furnishing_absorption_area(),
              scaled(row_area(FurnitureId::SofaTwoSeat), static_cast<double>(RoomScene::K_MAX_FURNISHINGS)));
}

TEST(Small_RoomScene, MediumAbsorptionIgnoresALiveLookingEntryPastTheCount)
{
    RoomScene scene = SushiDSP::test::garage_scene();
    scene.furnishings[0] = {FurnitureId::SofaTwoSeat, 1.0};
    scene.furnishings[1] = {FurnitureId::WardrobeModule, 3.0};
    scene.furnishing_count = 1;

    expect_sofa_coefficients(SushiDSP::room::MediumAbsorption(scene), 4.0 * scene.volume_m3());
}
