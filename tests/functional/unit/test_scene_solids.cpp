/**************************************************************************/
/* test_scene_solids.cpp                                                  */
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
 * @file test_scene_solids.cpp
 * @brief Tests the room scene's boxes, cone outlines and the microphone's aim.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "ui/scene_solids.hpp"

using SushiDSP::Room::Arrangement;
using SushiDSP::Room::Pose;
using SushiDSP::Room::SourcePlacement;

namespace
{
    /// @brief Holds a quarter turn, in radians.
    constexpr double K_QUARTER_TURN = 0.5 * 3.14159265358979323846;
} // namespace

TEST(Unit_Dsp, OrientedBoxSpansItsThreeSizesAboutItsCentre)
{
    const ui::SceneBox box = ui::oriented_box(ui::Point3{1.0, 2.0, 3.0}, 0.0, 4.0, 2.0, 1.0);

    for (std::size_t corner = 0; corner < 8; ++corner)
    {
        EXPECT_NEAR(std::fabs(box[corner].x - 1.0), 2.0, 1e-12);
        EXPECT_NEAR(std::fabs(box[corner].y - 2.0), 1.0, 1e-12);
        EXPECT_NEAR(std::fabs(box[corner].z - 3.0), 0.5, 1e-12);
    }
    EXPECT_LT(box[0].z, box[4].z);
}

TEST(Unit_Dsp, OrientedBoxTurnsItsLengthWithItsYaw)
{
    const ui::SceneBox box =
        ui::oriented_box(ui::Point3{0.0, 0.0, 0.0}, K_QUARTER_TURN, 4.0, 2.0, 1.0);

    for (std::size_t corner = 0; corner < 8; ++corner)
    {
        EXPECT_NEAR(std::fabs(box[corner].x), 1.0, 1e-9);
        EXPECT_NEAR(std::fabs(box[corner].y), 2.0, 1e-9);
    }
}

TEST(Unit_Dsp, CabinetBoxRunsBackFromTheBaffle)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};
    const ui::SceneBox box = ui::cabinet_box(pose, geometry);

    double front = -1e9;
    double back = 1e9;
    for (const ui::Point3& corner : box)
    {
        front = std::max(front, corner.x);
        back = std::min(back, corner.x);
        EXPECT_NEAR(std::fabs(corner.y - 2.0), 0.5 * geometry.baffle_width_m, 1e-12);
        EXPECT_NEAR(std::fabs(corner.z - 0.5), 0.5 * geometry.height_m, 1e-12);
    }
    EXPECT_NEAR(front, 2.0, 1e-12);
    EXPECT_NEAR(back, 2.0 - geometry.depth_m, 1e-12);
}

TEST(Unit_Dsp, ConeOutlineLiesOnTheBaffleAtTheConesRadius)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};

    for (std::uint32_t driver = 0; driver < geometry.driver_count(); ++driver)
    {
        const ui::Point3 centre = ui::cone_centre(pose, geometry, driver);
        for (const ui::Point3& point : ui::cone_outline(pose, geometry, driver))
        {
            EXPECT_NEAR(point.x, 2.0, 1e-12);
            const double radius = std::hypot(point.y - centre.y, point.z - centre.z);
            EXPECT_NEAR(radius, geometry.cone_radius_m, 1e-12);
        }
    }
}

TEST(Unit_Dsp, FurnishingBoxStandsOnTheFloorAtItsPlace)
{
    SushiDSP::room::RoomScene::Furnishing bed{SushiDSP::room::FurnitureId::BedDouble, 1.0, true,
                                              3.0, 2.0};
    const SushiDSP::room::FurnitureItem& row = SushiDSP::room::furniture(bed.item);
    const ui::SceneBox box = ui::furnishing_box(bed);

    EXPECT_NEAR(box[0].z, 0.0, 1e-12);
    EXPECT_NEAR(box[4].z, static_cast<double>(row.box_m[2]), 1e-6);
    for (const ui::Point3& corner : box)
    {
        EXPECT_NEAR(std::fabs(corner.x - 3.0), 0.5 * static_cast<double>(row.box_m[0]), 1e-6);
        EXPECT_NEAR(std::fabs(corner.y - 2.0), 0.5 * static_cast<double>(row.box_m[1]), 1e-6);
    }
}

TEST(Unit_Dsp, NearestConeFindsTheConeAMicrophoneIsPointedAt)
{
    Arrangement arrangement;
    std::string reason;
    SourcePlacement source;
    source.id = 7;
    source.pose = Pose{{2.0, 2.0, 0.5}, 0.0};
    source.cabinet = SushiDSP::room::K_CABINET4X12;
    ASSERT_TRUE(arrangement.add_source(source, reason)) << reason;

    // Five centimetres in front of cone 3's centre, two centimetres off its axis.
    const ui::Point3 centre = ui::cone_centre(source.pose, source.cabinet, 3u);
    const std::optional<ui::MicrophoneAim> aim =
        ui::nearest_cone(arrangement, {centre.x + 0.05, centre.y + 0.02, centre.z});

    ASSERT_TRUE(aim.has_value());
    EXPECT_EQ(aim->source, 0u);
    EXPECT_EQ(aim->driver, 3u);
    EXPECT_NEAR(aim->in_front_m, 0.05, 1e-12);
    EXPECT_NEAR(aim->off_axis_m, 0.02, 1e-9);
    EXPECT_NEAR(aim->distance_m, std::hypot(0.05, 0.02), 1e-12);
}

TEST(Unit_Dsp, NearestConeIsEmptyInARoomWithoutACabinet)
{
    const Arrangement arrangement;
    EXPECT_FALSE(ui::nearest_cone(arrangement, {1.0, 1.0, 1.0}).has_value());
}

TEST(Unit_Dsp, ConeLabelNamesEachConeAsSeenFromTheFront)
{
    const SushiDSP::room::CabinetGeometry& four = SushiDSP::room::K_CABINET4X12;
    // Facing +x the cabinet's left is +y, which someone facing the baffle has on their right.
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};
    for (std::uint32_t driver = 0; driver < 4; ++driver)
    {
        const ui::Point3 centre = ui::cone_centre(pose, four, driver);
        const std::string label = ui::cone_label(four, driver);
        EXPECT_EQ(label.find("top") != std::string::npos, centre.z > 0.5) << label;
        EXPECT_EQ(label.find("right") != std::string::npos, centre.y > 2.0) << label;
    }
    EXPECT_STREQ(ui::cone_label(SushiDSP::room::K_CABINET1X12, 0), "centre");
    EXPECT_STRNE(ui::cone_label(SushiDSP::room::K_CABINET2X12, 0),
                 ui::cone_label(SushiDSP::room::K_CABINET2X12, 1));
}

TEST(Unit_Dsp, MicrophoneOnConeStandsWhereNearestConeThenReadsIt)
{
    Arrangement arrangement;
    std::string reason;
    SourcePlacement source;
    source.id = 3;
    source.pose = Pose{{2.0, 2.0, 0.6}, 0.7};
    ASSERT_TRUE(arrangement.add_source(source, reason)) << reason;

    for (std::uint32_t driver = 0; driver < 4; ++driver)
    {
        const Pose placed =
            ui::microphone_on_cone(source.pose, source.cabinet, driver, 0.04, 0.03, -0.02);
        const std::optional<ui::MicrophoneAim> aim =
            ui::nearest_cone(arrangement, placed.position);
        ASSERT_TRUE(aim.has_value());
        EXPECT_EQ(aim->driver, driver);
        EXPECT_NEAR(aim->in_front_m, 0.04, 1e-12);
        EXPECT_NEAR(aim->aside_m, 0.03, 1e-12);
        EXPECT_NEAR(aim->above_m, -0.02, 1e-12);
        // The capsule looks back at the baffle.
        EXPECT_NEAR(std::cos(placed.yaw_rad - source.pose.yaw_rad), -1.0, 1e-12);
    }
}
