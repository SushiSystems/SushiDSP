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
 * @file test_room_arrangement.cpp
 * @brief Tests the room arrangement's limits, edits and pair scenes.
 * @author Mustafa Garip & Sushi Systems
 */

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
} // namespace

TEST(Unit_Dsp, ArrangementStartsWithTheStarterRoomAndNothingPlaced)
{
    const Arrangement arrangement;

    EXPECT_TRUE(arrangement.sources().empty());
    EXPECT_TRUE(arrangement.microphones().empty());
    EXPECT_EQ(arrangement.shell().dimensions.x, SushiDSP::room::starter_scene().dimensions.x);
    std::string reason;
    EXPECT_TRUE(arrangement.is_valid(reason)) << reason;
}

TEST(Unit_Dsp, ArrangementFromASceneHoldsItsCabinetAndListener)
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

TEST(Unit_Dsp, ArrangementRefusesAFourthCabinetAndStaysAsItWas)
{
    Arrangement arrangement;
    std::string reason;
    for (std::uint64_t id = 1; id <= Arrangement::K_MAX_SOURCES; ++id)
    {
        ASSERT_TRUE(arrangement.add_source(SourcePlacement{id, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason))
            << reason;
    }

    EXPECT_FALSE(arrangement.add_source(SourcePlacement{9, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));

    EXPECT_NE(reason.find("at most 3 cabinets"), std::string::npos) << reason;
    EXPECT_EQ(arrangement.sources().size(), Arrangement::K_MAX_SOURCES);
    EXPECT_EQ(arrangement.find_source(9), nullptr);
}

TEST(Unit_Dsp, ArrangementRefusesANinthMicrophoneAndStaysAsItWas)
{
    Arrangement arrangement;
    std::string reason;
    for (std::uint64_t id = 1; id <= Arrangement::K_MAX_MICROPHONES; ++id)
    {
        ASSERT_TRUE(arrangement.add_microphone(microphone_at(id, 1.0), reason)) << reason;
    }

    EXPECT_FALSE(arrangement.add_microphone(microphone_at(99, 1.0), reason));

    EXPECT_NE(reason.find("at most 8 microphones"), std::string::npos) << reason;
    EXPECT_EQ(arrangement.microphones().size(), Arrangement::K_MAX_MICROPHONES);
    EXPECT_EQ(arrangement.find_microphone(99), nullptr);
}

TEST(Unit_Dsp, ArrangementRefusesAnIdentityItAlreadyHolds)
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

TEST(Unit_Dsp, ArrangementMovesUpdatesAndRemovesByIdentity)
{
    Arrangement arrangement;
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{1, Pose{{1.0, 1.0, 0.5}, 0.0}}, reason));
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{2, Pose{{1.0, 2.0, 0.5}, 0.0}}, reason));
    ASSERT_TRUE(arrangement.add_microphone(microphone_at(5, 1.0), reason));
    ASSERT_TRUE(arrangement.add_microphone(microphone_at(6, 1.1), reason));

    EXPECT_TRUE(arrangement.move_source(2, Pose{{3.0, 2.0, 0.5}, 1.0}));
    EXPECT_FALSE(arrangement.move_source(4, Pose{}));
    MicrophonePlacement changed = microphone_at(6, 1.4);
    changed.pattern = SushiDSP::room::PolarPattern::FigureEight;
    changed.proximity = false;
    EXPECT_TRUE(arrangement.update_microphone(changed));
    EXPECT_FALSE(arrangement.update_microphone(microphone_at(8, 1.0)));

    EXPECT_EQ(arrangement.find_source(2)->pose.position.x, 3.0);
    EXPECT_EQ(arrangement.find_microphone(6)->pose.position.z, 1.4);
    EXPECT_EQ(arrangement.find_microphone(6)->pattern, SushiDSP::room::PolarPattern::FigureEight);
    EXPECT_FALSE(arrangement.find_microphone(6)->proximity);

    // Removing the first of each moves the second down one slot.
    EXPECT_TRUE(arrangement.remove_source(1));
    EXPECT_TRUE(arrangement.remove_microphone(5));
    EXPECT_FALSE(arrangement.remove_source(1));
    ASSERT_EQ(arrangement.sources().size(), 1u);
    ASSERT_EQ(arrangement.microphones().size(), 1u);
    EXPECT_EQ(arrangement.sources()[0].id, 2u);
    EXPECT_EQ(arrangement.microphones()[0].id, 6u);
}

TEST(Unit_Dsp, ArrangementRenamesAnIdentityWhereverItIsHeld)
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

TEST(Unit_Dsp, ArrangementBuildsEachPairsSceneFromTheShellAndTheTwoPoses)
{
    Arrangement arrangement;
    arrangement.set_shell(SushiDSP::test::garage_scene());
    arrangement.set_listener(Pose{{4.0, 2.0, 1.2}, 0.5});
    std::string reason;
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{1, Pose{{1.0, 1.0, 0.5}, 0.1}}, reason));
    ASSERT_TRUE(arrangement.add_source(SourcePlacement{2, Pose{{1.0, 3.0, 0.5}, 0.2}}, reason));
    MicrophonePlacement microphone = microphone_at(5, 1.0);
    microphone.pattern = SushiDSP::room::PolarPattern::Omni;
    microphone.proximity = false;
    ASSERT_TRUE(arrangement.add_microphone(microphone, reason));

    const SushiDSP::room::RoomScene ears = arrangement.pair_scene(1, 0);
    const SushiDSP::room::RoomScene capsule = arrangement.pair_scene(0, 1);

    EXPECT_EQ(ears.dimensions.x, 6.0);
    EXPECT_EQ(ears.source_position.y, 3.0);
    EXPECT_EQ(ears.source_yaw_rad, 0.2);
    EXPECT_EQ(ears.listener_position.x, 4.0);
    EXPECT_EQ(ears.listener_yaw_rad, 0.5);
    EXPECT_EQ(capsule.source_position.y, 1.0);
    EXPECT_EQ(capsule.microphone_position.z, 1.0);
    EXPECT_EQ(capsule.microphone_pattern, SushiDSP::room::PolarPattern::Omni);
    EXPECT_FALSE(capsule.microphone_proximity);
}

TEST(Unit_Dsp, ArrangementIsRefusedWhenAMicrophoneStandsOutsideTheShell)
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

TEST(Unit_Dsp, ScenesEqualComparesEveryFieldASolveReads)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    EXPECT_TRUE(SushiDSP::Room::scenes_equal(scene, scene));

    SushiDSP::room::RoomScene moved = scene;
    moved.microphone_position.x += 0.01;
    EXPECT_FALSE(SushiDSP::Room::scenes_equal(scene, moved));

    SushiDSP::room::RoomScene patched = scene;
    patched.patches[0].area_m2 += 0.5;
    EXPECT_FALSE(SushiDSP::Room::scenes_equal(scene, patched));

    SushiDSP::room::RoomScene close_up = scene;
    close_up.microphone_proximity = false;
    EXPECT_FALSE(SushiDSP::Room::scenes_equal(scene, close_up));
}

TEST(Unit_Dsp, ArrangementGivesEachCabinetItsOwnGeometry)
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
    EXPECT_FALSE(SushiDSP::Room::shells_equal(arrangement.pair_scene(0, 0),
                                              arrangement.pair_scene(1, 0)));

    ASSERT_TRUE(arrangement.set_source_cabinet(1u, SushiDSP::room::K_CABINET2X12));
    EXPECT_EQ(arrangement.pair_scene(0, 0).cabinet.driver_rows, 1u);
    EXPECT_FALSE(arrangement.set_source_cabinet(9u, SushiDSP::room::K_CABINET2X12));
}

TEST(Unit_Dsp, ShellsEqualIgnoresWhoStandsWhereAndReadsAFurnishingsPlace)
{
    SushiDSP::room::RoomScene left = SushiDSP::room::starter_scene();
    SushiDSP::room::RoomScene right = left;
    right.source_position.x += 0.5;
    right.listener_yaw_rad += 0.3;
    right.microphone_position.z += 0.2;
    EXPECT_TRUE(SushiDSP::Room::shells_equal(left, right));
    EXPECT_FALSE(SushiDSP::Room::scenes_equal(left, right));

    right.furnishing_count = 1;
    left.furnishing_count = 1;
    left.furnishings[0] = {SushiDSP::room::FurnitureId::Desk, 1.0};
    right.furnishings[0] = {SushiDSP::room::FurnitureId::Desk, 1.0, true, 2.0, 2.0};
    EXPECT_FALSE(SushiDSP::Room::shells_equal(left, right));
}
