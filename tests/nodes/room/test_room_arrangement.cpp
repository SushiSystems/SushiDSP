/**************************************************************************/
/* test_room_arrangement.cpp                                              */
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
 * @file test_room_arrangement.cpp
 * @brief Tests the room arrangement's limits, edits and pair scenes.
 * @author Mustafa Garip & Sushi Systems
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/starter_scene.hpp>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

using SushiDSP::Room::Arrangement;
using SushiDSP::Room::MicrophonePlacement;
using SushiDSP::Room::Pose;
using SushiDSP::Room::SourcePlacement;

namespace
{
    /// @brief Returns a microphone under one identity at one height above the garage's floor.
    MicrophonePlacement microphone_at(std::uint64_t id, double z)
    {
        MicrophonePlacement microphone;
        microphone.id = id;
        microphone.pose = Pose{{2.0, 2.0, z}, 0.0};
        return microphone;
    }

    /// @brief Returns an arrangement holding the given number of cabinets, numbered from 1.
    Arrangement with_sources(std::size_t count)
    {
        Arrangement arrangement;
        std::string reason;
        for (std::uint64_t id = 1; id <= count; ++id)
        {
            static_cast<void>(
                arrangement.add_source(SourcePlacement{id, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));
        }
        return arrangement;
    }

    /// @brief Returns an arrangement holding the given number of microphones, numbered from 1.
    Arrangement with_microphones(std::size_t count)
    {
        Arrangement arrangement;
        std::string reason;
        for (std::uint64_t id = 1; id <= count; ++id)
        {
            static_cast<void>(arrangement.add_microphone(microphone_at(id, 1.0), reason));
        }
        return arrangement;
    }

    /// @brief Returns the garage with one desk placed in it, which every row below changes one
    /// field of.
    SushiDSP::room::RoomScene furnished_baseline()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.furnishing_count = 1;
        scene.furnishings[0] = {SushiDSP::room::FurnitureId::Desk, 1.0};
        return scene;
    }

    /// @brief Holds one field a solve reads and a change of it.
    struct FieldChange
    {
            const char* name;
            void (*change)(SushiDSP::room::RoomScene&);
    };

    /// @brief Holds one change per field of the scene that scenes_equal compares. The kind of
    /// microphone has one value, so it has no row.
    const std::array<FieldChange, 27> kFieldChanges = {{
        {"Dimensions", [](SushiDSP::room::RoomScene& scene) { scene.dimensions.x += 0.5; }},
        {"Surfaces",
         [](SushiDSP::room::RoomScene& scene)
         { scene.surfaces[0] = SushiDSP::room::MaterialId::GlassWindow; }},
        {"Scattering",
         [](SushiDSP::room::RoomScene& scene)
         { scene.scattering[0] = SushiDSP::room::Scattering::Textured; }},
        {"PatchCount", [](SushiDSP::room::RoomScene& scene) { scene.patch_count = 0; }},
        {"FurnishingCount", [](SushiDSP::room::RoomScene& scene) { scene.furnishing_count = 0; }},
        {"BuiltRoomLoss",
         [](SushiDSP::room::RoomScene& scene) { scene.built_room_loss = !scene.built_room_loss; }},
        {"PatchSurface",
         [](SushiDSP::room::RoomScene& scene)
         { scene.patches[0].surface = SushiDSP::room::Surface::MinusX; }},
        {"PatchMaterial",
         [](SushiDSP::room::RoomScene& scene)
         { scene.patches[0].material = SushiDSP::room::MaterialId::GlassWindow; }},
        {"PatchArea", [](SushiDSP::room::RoomScene& scene) { scene.patches[0].area_m2 += 0.5; }},
        {"FurnishingItem",
         [](SushiDSP::room::RoomScene& scene)
         { scene.furnishings[0].item = SushiDSP::room::FurnitureId::BedDouble; }},
        {"FurnishingQuantity",
         [](SushiDSP::room::RoomScene& scene) { scene.furnishings[0].quantity += 1.0; }},
        {"FurnishingPlaced",
         [](SushiDSP::room::RoomScene& scene)
         { scene.furnishings[0].placed = !scene.furnishings[0].placed; }},
        {"FurnishingX", [](SushiDSP::room::RoomScene& scene) { scene.furnishings[0].x_m += 0.5; }},
        {"FurnishingY", [](SushiDSP::room::RoomScene& scene) { scene.furnishings[0].y_m += 0.5; }},
        {"FurnishingYaw",
         [](SushiDSP::room::RoomScene& scene) { scene.furnishings[0].yaw_rad += 0.5; }},
        {"Cabinet",
         [](SushiDSP::room::RoomScene& scene) { scene.cabinet = SushiDSP::room::K_CABINET1X12; }},
        {"SourcePosition", [](SushiDSP::room::RoomScene& scene) { scene.source_position.x += 0.5; }},
        {"SourceYaw", [](SushiDSP::room::RoomScene& scene) { scene.source_yaw_rad += 0.5; }},
        {"ListenerPosition",
         [](SushiDSP::room::RoomScene& scene) { scene.listener_position.x += 0.5; }},
        {"ListenerYaw", [](SushiDSP::room::RoomScene& scene) { scene.listener_yaw_rad += 0.5; }},
        {"MicrophonePosition",
         [](SushiDSP::room::RoomScene& scene) { scene.microphone_position.x += 0.01; }},
        {"MicrophoneYaw", [](SushiDSP::room::RoomScene& scene) { scene.microphone_yaw_rad += 0.5; }},
        {"MicrophonePattern",
         [](SushiDSP::room::RoomScene& scene)
         {
             scene.microphone_pattern = scene.microphone_pattern == SushiDSP::room::PolarPattern::Omni
                                            ? SushiDSP::room::PolarPattern::Cardioid
                                            : SushiDSP::room::PolarPattern::Omni;
         }},
        {"MicrophoneProximity",
         [](SushiDSP::room::RoomScene& scene)
         { scene.microphone_proximity = !scene.microphone_proximity; }},
        {"SourcePositionY", [](SushiDSP::room::RoomScene& scene) { scene.source_position.y += 0.5; }},
        {"ListenerPositionZ",
         [](SushiDSP::room::RoomScene& scene) { scene.listener_position.z += 0.5; }},
        {"MicrophonePositionZ",
         [](SushiDSP::room::RoomScene& scene) { scene.microphone_position.z += 0.5; }},
    }};

    /// @brief Names a field change after its field.
    std::string field_name(const ::testing::TestParamInfo<FieldChange>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_RoomArrangementScenes : public ::testing::TestWithParam<FieldChange>
{
};

TEST(Small_RoomArrangement, ArrangementStartsValid)
{
    const Arrangement arrangement;
    std::string reason;
    EXPECT_TRUE(arrangement.is_valid(reason)) << reason;
}

TEST(Small_RoomArrangement, ArrangementFromASceneHoldsItsCabinetAndListener)
{
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position = {1.0, 1.5, 0.5};
    scene.listener_position = {4.0, 2.5, 1.2};
    scene.listener_yaw_rad = 0.25;

    const Arrangement arrangement = Arrangement::from_scene(scene);

    ASSERT_EQ(arrangement.sources().size(), 1u);
    EXPECT_EQ(arrangement.sources()[0].id, 0u);
    EXPECT_EQ(arrangement.sources()[0].pose.position.y, 1.5);
    EXPECT_EQ(arrangement.listener().position.x, 4.0);
    EXPECT_EQ(arrangement.listener().yaw_rad, 0.25);
    EXPECT_TRUE(arrangement.microphones().empty());
}

TEST(Small_RoomArrangement, ArrangementRefusesAFourthCabinetAndStaysAsItWas)
{
    Arrangement arrangement = with_sources(Arrangement::K_MAX_SOURCES);
    std::string reason;

    EXPECT_FALSE(arrangement.add_source(SourcePlacement{9, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));

    EXPECT_NE(reason.find("at most 3 cabinets"), std::string::npos) << reason;
    EXPECT_EQ(arrangement.sources().size(), Arrangement::K_MAX_SOURCES);
    EXPECT_EQ(arrangement.find_source(9), nullptr);
}

TEST(Small_RoomArrangement, ArrangementRefusesANinthMicrophoneAndStaysAsItWas)
{
    Arrangement arrangement = with_microphones(Arrangement::K_MAX_MICROPHONES);
    std::string reason;

    EXPECT_FALSE(arrangement.add_microphone(microphone_at(99, 1.0), reason));

    EXPECT_NE(reason.find("at most 8 microphones"), std::string::npos) << reason;
    EXPECT_EQ(arrangement.microphones().size(), Arrangement::K_MAX_MICROPHONES);
    EXPECT_EQ(arrangement.find_microphone(99), nullptr);
}

TEST(Small_RoomArrangement, ArrangementRefusesAnIdentityItAlreadyHolds)
{
    Arrangement arrangement;
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{7, Pose{}}, reason));
    ASSERT_TRUE(arrangement.add_microphone(microphone_at(7, 1.0), reason));

    EXPECT_FALSE(arrangement.add_source(SourcePlacement{7, Pose{}}, reason));
    EXPECT_FALSE(arrangement.add_microphone(microphone_at(7, 1.2), reason));
    EXPECT_EQ(arrangement.sources().size(), 1u);
    EXPECT_EQ(arrangement.microphones().size(), 1u);
}

class Small_RoomArrangementPlaced : public ::testing::Test
{
    protected:
        /// @brief Places cabinets 1 and 2 and microphones 5 and 6.
        void SetUp() override
        {
            std::string reason;
            ASSERT_TRUE(arrangement.add_source(SourcePlacement{1, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));
            ASSERT_TRUE(arrangement.add_source(SourcePlacement{2, Pose{{1.0, 2.0, 0.5}, 0.0}}, reason));
            ASSERT_TRUE(arrangement.add_microphone(microphone_at(5, 1.0), reason));
            ASSERT_TRUE(arrangement.add_microphone(microphone_at(6, 1.1), reason));
        }

        /// @brief Stores the arrangement under test.
        Arrangement arrangement;
};

TEST_F(Small_RoomArrangementPlaced, MoveSourceMovesTheCabinetOfThatIdentity)
{
    EXPECT_TRUE(arrangement.move_source(2, Pose{{3.0, 2.0, 0.5}, 1.0}));
    EXPECT_EQ(arrangement.find_source(2)->pose.position.x, 3.0);
}

TEST_F(Small_RoomArrangementPlaced, MoveSourceRefusesAnUnknownIdentity)
{
    EXPECT_FALSE(arrangement.move_source(4, Pose{}));
}

TEST_F(Small_RoomArrangementPlaced, UpdateMicrophoneReplacesTheMicrophoneOfThatIdentity)
{
    MicrophonePlacement changed = microphone_at(6, 1.4);
    changed.pattern = SushiDSP::room::PolarPattern::FigureEight;
    changed.proximity = false;
    EXPECT_TRUE(arrangement.update_microphone(changed));
    EXPECT_EQ(arrangement.find_microphone(6)->pose.position.z, 1.4);
    EXPECT_EQ(arrangement.find_microphone(6)->pattern, SushiDSP::room::PolarPattern::FigureEight);
    EXPECT_FALSE(arrangement.find_microphone(6)->proximity);
}

TEST_F(Small_RoomArrangementPlaced, UpdateMicrophoneRefusesAnUnknownIdentity)
{
    EXPECT_FALSE(arrangement.update_microphone(microphone_at(8, 1.0)));
}

TEST_F(Small_RoomArrangementPlaced, RemovingTheFirstCabinetMovesTheSecondDownOneSlot)
{
    // Removing the first of each moves the second down one slot.
    EXPECT_TRUE(arrangement.remove_source(1));
    ASSERT_EQ(arrangement.sources().size(), 1u);
    EXPECT_EQ(arrangement.sources()[0].id, 2u);
}

TEST_F(Small_RoomArrangementPlaced, RemovingTheFirstMicrophoneMovesTheSecondDownOneSlot)
{
    EXPECT_TRUE(arrangement.remove_microphone(5));
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.microphones()[0].id, 6u);
}

TEST_F(Small_RoomArrangementPlaced, RemoveSourceRefusesAnIdentityAlreadyRemoved)
{
    ASSERT_TRUE(arrangement.remove_source(1));
    EXPECT_FALSE(arrangement.remove_source(1));
}

TEST(Small_RoomArrangement, ArrangementRenamesAnIdentityWhereverItIsHeld)
{
    Arrangement arrangement;
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{3, Pose{}}, reason));
    ASSERT_TRUE(arrangement.add_microphone(microphone_at(4, 1.0), reason));

    arrangement.rename(3, 30);
    arrangement.rename(4, 40);

    EXPECT_NE(arrangement.find_source(30), nullptr);
    EXPECT_EQ(arrangement.find_source(3), nullptr);
    EXPECT_NE(arrangement.find_microphone(40), nullptr);
    EXPECT_EQ(arrangement.find_microphone(4), nullptr);
}

class Small_RoomArrangementPairs : public ::testing::Test
{
    protected:
        /// @brief Holds the garage with the listener at (4, 2, 1.2), cabinets 1 and 2 and one omni
        /// microphone without proximity.
        void SetUp() override
        {
            arrangement.set_shell(SushiDSP::test::garage_scene());
            arrangement.set_listener(Pose{{4.0, 2.0, 1.2}, 0.5});
            std::string reason;
            ASSERT_TRUE(
                arrangement.add_source(SourcePlacement{1, Pose{{1.0, 1.0, 0.5}, 0.1}}, reason));
            ASSERT_TRUE(
                arrangement.add_source(SourcePlacement{2, Pose{{1.0, 3.0, 0.5}, 0.2}}, reason));
            MicrophonePlacement microphone = microphone_at(5, 1.0);
            microphone.pattern = SushiDSP::room::PolarPattern::Omni;
            microphone.proximity = false;
            ASSERT_TRUE(arrangement.add_microphone(microphone, reason));
        }

        /// @brief Stores the arrangement under test.
        Arrangement arrangement;
};

TEST_F(Small_RoomArrangementPairs, EarsPairSceneTakesTheShellTheSecondCabinetAndTheListener)
{
    const SushiDSP::room::RoomScene ears = arrangement.pair_scene(1, 0);

    EXPECT_EQ(ears.dimensions.x, 6.0);
    EXPECT_EQ(ears.source_position.y, 3.0);
    EXPECT_EQ(ears.source_yaw_rad, 0.2);
    EXPECT_EQ(ears.listener_position.x, 4.0);
    EXPECT_EQ(ears.listener_yaw_rad, 0.5);
}

TEST_F(Small_RoomArrangementPairs, CapsulePairSceneTakesTheFirstCabinetAndTheMicrophone)
{
    const SushiDSP::room::RoomScene capsule = arrangement.pair_scene(0, 1);

    EXPECT_EQ(capsule.source_position.y, 1.0);
    EXPECT_EQ(capsule.microphone_position.z, 1.0);
    EXPECT_EQ(capsule.microphone_pattern, SushiDSP::room::PolarPattern::Omni);
    EXPECT_FALSE(capsule.microphone_proximity);
}

TEST(Small_RoomArrangement, ArrangementIsRefusedWhenAMicrophoneStandsOutsideTheShell)
{
    Arrangement arrangement;
    arrangement.set_shell(SushiDSP::test::garage_scene());
    arrangement.set_listener(Pose{{4.0, 2.0, 1.2}, 0.0});
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{1, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));
    ASSERT_TRUE(arrangement.add_microphone(microphone_at(5, 1.0), reason));
    ASSERT_TRUE(arrangement.is_valid(reason)) << reason;

    // The garage is 2.5 m high.
    ASSERT_TRUE(arrangement.update_microphone(microphone_at(5, 9.0)));

    EXPECT_FALSE(arrangement.is_valid(reason));
    EXPECT_NE(reason.find("microphone 1"), std::string::npos) << reason;
}

TEST(Small_RoomArrangement, ScenesEqualIsTrueForTheSameScene)
{
    const SushiDSP::room::RoomScene scene = furnished_baseline();
    EXPECT_TRUE(SushiDSP::Room::scenes_equal(scene, scene));
}

TEST_P(Small_RoomArrangementScenes, ScenesEqualSeesAChangeOfEveryFieldASolveReads)
{
    const SushiDSP::room::RoomScene scene = furnished_baseline();
    SushiDSP::room::RoomScene changed = scene;
    GetParam().change(changed);
    EXPECT_FALSE(SushiDSP::Room::scenes_equal(scene, changed));
}

INSTANTIATE_TEST_SUITE_P(Fields, Small_RoomArrangementScenes, ::testing::ValuesIn(kFieldChanges),
                         field_name);

TEST(Small_RoomArrangement, PairScenesTakeTheirOwnCabinetsGeometry)
{
    Arrangement arrangement;
    std::string reason;
    SourcePlacement first{1u, Pose{{1.5, 1.0, 0.5}, 0.0}};
    SourcePlacement second{2u, Pose{{1.5, 3.0, 0.5}, 0.0}, SushiDSP::room::K_CABINET1X12};
    ASSERT_TRUE(arrangement.add_source(first, reason)) << reason;
    ASSERT_TRUE(arrangement.add_source(second, reason)) << reason;

    EXPECT_TRUE(SushiDSP::room::same_geometry(arrangement.pair_scene(0, 0).cabinet,
                                              SushiDSP::room::K_CABINET4X12));
    EXPECT_TRUE(SushiDSP::room::same_geometry(arrangement.pair_scene(1, 0).cabinet,
                                              SushiDSP::room::K_CABINET1X12));
}

TEST(Small_RoomArrangement, ShellsDifferWhenTheirCabinetsDo)
{
    Arrangement arrangement;
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{1u, Pose{{1.5, 1.0, 0.5}, 0.0}}, reason));
    ASSERT_TRUE(arrangement.add_source(
        SourcePlacement{2u, Pose{{1.5, 3.0, 0.5}, 0.0}, SushiDSP::room::K_CABINET1X12}, reason));

    EXPECT_FALSE(SushiDSP::Room::shells_equal(arrangement.pair_scene(0, 0),
                                              arrangement.pair_scene(1, 0)));
}

TEST(Small_RoomArrangement, SetSourceCabinetChangesTheCabinetOfThatIdentity)
{
    Arrangement arrangement = with_sources(2);

    ASSERT_TRUE(arrangement.set_source_cabinet(1u, SushiDSP::room::K_CABINET2X12));

    EXPECT_EQ(arrangement.pair_scene(0, 0).cabinet.driver_rows, 1u);
}

TEST(Small_RoomArrangement, SetSourceCabinetRefusesAnUnknownIdentity)
{
    Arrangement arrangement = with_sources(2);
    EXPECT_FALSE(arrangement.set_source_cabinet(9u, SushiDSP::room::K_CABINET2X12));
}

TEST(Small_RoomArrangement, ShellsEqualIgnoresWhoStandsWhere)
{
    const SushiDSP::room::RoomScene left = SushiDSP::room::starter_scene();
    SushiDSP::room::RoomScene right = left;
    right.source_position.x += 0.5;
    right.listener_yaw_rad += 0.3;
    right.microphone_position.z += 0.2;
    EXPECT_TRUE(SushiDSP::Room::shells_equal(left, right));
}

TEST(Small_RoomArrangement, ShellsEqualReadsAFurnishingsPlace)
{
    SushiDSP::room::RoomScene left = SushiDSP::room::starter_scene();
    SushiDSP::room::RoomScene right = left;
    right.furnishing_count = 1;
    left.furnishing_count = 1;
    left.furnishings[0] = {SushiDSP::room::FurnitureId::Desk, 1.0};
    right.furnishings[0] = {SushiDSP::room::FurnitureId::Desk, 1.0, true, 2.0, 2.0};
    EXPECT_FALSE(SushiDSP::Room::shells_equal(left, right));
}
