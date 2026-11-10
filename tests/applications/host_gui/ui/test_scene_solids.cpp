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
 * @file test_scene_solids.cpp
 * @brief Tests the room scene's boxes, cone outlines and the microphone's aim.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "support/tolerance.hpp"
#include "ui/scene_solids.hpp"

using SushiDSP::Room::Arrangement;
using SushiDSP::Room::Pose;
using SushiDSP::Room::SourcePlacement;

namespace
{
    /// @brief Holds a quarter turn, in radians.
    constexpr double K_QUARTER_TURN = 0.5 * 3.14159265358979323846;

    /// @brief Rounded double operations on the longest path from a pose to a corner or a cone
    /// point: the sine and the cosine of the yaw, two products, their sum and the offset.
    constexpr std::size_t K_GEOMETRY_OPERATIONS = 8;

    /// @brief The largest coordinate magnitude, in metres, any case works at; the rounding of a
    /// coordinate is at most u times this.
    constexpr double K_ROOM_SCALE = 4.0;

    /// @brief The bound, in metres, on a coordinate built by K_GEOMETRY_OPERATIONS roundings.
    double geometry_bound()
    {
        return static_cast<double>(K_GEOMETRY_OPERATIONS) * TestSupport::unit_roundoff<double>() *
               K_ROOM_SCALE;
    }

    /// @brief Checks a value against a reference within the geometry bound.
    ::testing::AssertionResult near_geometry(double computed, double reference)
    {
        return TestSupport::is_within(
            TestSupport::measure_error(computed, reference, geometry_bound()));
    }

    /// @brief Checks that every corner of a box lies half a size either side of a centre on one axis.
    /// @param box The box.
    /// @param axis The coordinate to check.
    /// @param centre The centre's coordinate.
    /// @param half The half size.
    ::testing::AssertionResult corners_straddle(const ui::SceneBox& box, double ui::Point3::*axis,
                                                double centre, double half)
    {
        for (std::size_t corner = 0; corner < box.size(); ++corner)
        {
            ::testing::AssertionResult result =
                near_geometry(std::fabs(box[corner].*axis - centre), half);
            if (!result)
            {
                return result << " at corner " << corner;
            }
        }
        return ::testing::AssertionSuccess();
    }

    /// @brief The largest value of one coordinate over a box's corners.
    double highest(const ui::SceneBox& box, double ui::Point3::*axis)
    {
        double best = box[0].*axis;
        for (const ui::Point3& corner : box)
        {
            best = std::max(best, corner.*axis);
        }
        return best;
    }

    /// @brief The smallest value of one coordinate over a box's corners.
    double lowest(const ui::SceneBox& box, double ui::Point3::*axis)
    {
        double best = box[0].*axis;
        for (const ui::Point3& corner : box)
        {
            best = std::min(best, corner.*axis);
        }
        return best;
    }

    /// @brief The largest distance of a cone outline's points from the baffle plane x = 2.
    double worst_baffle_miss(const Pose& pose, const SushiDSP::room::CabinetGeometry& geometry,
                             std::uint32_t driver)
    {
        double worst = 0.0;
        for (const ui::Point3& point : ui::cone_outline(pose, geometry, driver))
        {
            worst = std::max(worst, std::fabs(point.x - 2.0));
        }
        return worst;
    }

    /// @brief The largest miss of a cone outline's points from the cone's radius about its centre.
    double worst_radius_miss(const Pose& pose, const SushiDSP::room::CabinetGeometry& geometry,
                             std::uint32_t driver)
    {
        const ui::Point3 centre = ui::cone_centre(pose, geometry, driver);
        double worst = 0.0;
        for (const ui::Point3& point : ui::cone_outline(pose, geometry, driver))
        {
            const double radius = std::hypot(point.y - centre.y, point.z - centre.z);
            worst = std::max(worst, std::fabs(radius - geometry.cone_radius_m));
        }
        return worst;
    }

    /// @brief One driver of the four-by-twelve cabinet.
    struct DriverRow
    {
            const char* name;
            std::uint32_t driver;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string driver_row_name(const ::testing::TestParamInfo<DriverRow>& info)
    {
        return std::string(info.param.name);
    }

    /// @brief The four drivers of the four-by-twelve cabinet.
    const std::vector<DriverRow>& four_drivers()
    {
        static const std::vector<DriverRow> rows{
            {"Driver0", 0u}, {"Driver1", 1u}, {"Driver2", 2u}, {"Driver3", 3u}};
        return rows;
    }
} // namespace

TEST(Small_SceneSolids, OrientedBoxSpansItsThreeSizesAboutItsCentre)
{
    const ui::SceneBox box = ui::oriented_box(ui::Point3{1.0, 2.0, 3.0}, 0.0, 4.0, 2.0, 1.0);

    EXPECT_TRUE(corners_straddle(box, &ui::Point3::x, 1.0, 2.0));
    EXPECT_TRUE(corners_straddle(box, &ui::Point3::y, 2.0, 1.0));
    EXPECT_TRUE(corners_straddle(box, &ui::Point3::z, 3.0, 0.5));
    EXPECT_LT(box[0].z, box[4].z);
}

TEST(Small_SceneSolids, OrientedBoxTurnsItsLengthWithItsYaw)
{
    const ui::SceneBox box =
        ui::oriented_box(ui::Point3{0.0, 0.0, 0.0}, K_QUARTER_TURN, 4.0, 2.0, 1.0);

    EXPECT_TRUE(corners_straddle(box, &ui::Point3::x, 0.0, 1.0));
    EXPECT_TRUE(corners_straddle(box, &ui::Point3::y, 0.0, 2.0));
}

TEST(Small_SceneSolids, CabinetBoxRunsBackFromTheBaffle)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};
    const ui::SceneBox box = ui::cabinet_box(pose, geometry);

    EXPECT_TRUE(near_geometry(highest(box, &ui::Point3::x), 2.0));
    EXPECT_TRUE(near_geometry(lowest(box, &ui::Point3::x), 2.0 - geometry.depth_m));
}

TEST(Small_SceneSolids, CabinetBoxSpansTheBaffleWidthAndTheHeight)
{
    const SushiDSP::room::CabinetGeometry& geometry = SushiDSP::room::K_CABINET4X12;
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};
    const ui::SceneBox box = ui::cabinet_box(pose, geometry);

    EXPECT_TRUE(corners_straddle(box, &ui::Point3::y, 2.0, 0.5 * geometry.baffle_width_m));
    EXPECT_TRUE(corners_straddle(box, &ui::Point3::z, 0.5, 0.5 * geometry.height_m));
}

class Small_SceneSolidsCone : public ::testing::TestWithParam<DriverRow>
{
};

TEST_P(Small_SceneSolidsCone, ConeOutlineLiesOnTheBaffle)
{
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};

    EXPECT_TRUE(near_geometry(
        worst_baffle_miss(pose, SushiDSP::room::K_CABINET4X12, GetParam().driver), 0.0));
}

TEST_P(Small_SceneSolidsCone, ConeOutlineLiesAtTheConesRadius)
{
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};

    EXPECT_TRUE(near_geometry(
        worst_radius_miss(pose, SushiDSP::room::K_CABINET4X12, GetParam().driver), 0.0));
}

TEST_P(Small_SceneSolidsCone, ConeLabelNamesTheConeAsSeenFromTheFront)
{
    const SushiDSP::room::CabinetGeometry& four = SushiDSP::room::K_CABINET4X12;
    // Facing +x the cabinet's left is +y, which someone facing the baffle has on their right.
    const Pose pose{{2.0, 2.0, 0.5}, 0.0};
    const ui::Point3 centre = ui::cone_centre(pose, four, GetParam().driver);
    const std::string label = ui::cone_label(four, GetParam().driver);

    EXPECT_EQ(label.find("top") != std::string::npos, centre.z > 0.5) << label;
    EXPECT_EQ(label.find("right") != std::string::npos, centre.y > 2.0) << label;
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_SceneSolidsCone, ::testing::ValuesIn(four_drivers()),
                         driver_row_name);

TEST(Small_SceneSolids, ConeLabelNamesTheOnlyDriverOfASingleCabinetCentre)
{
    EXPECT_STREQ(ui::cone_label(SushiDSP::room::K_CABINET1X12, 0), "centre");
}

TEST(Small_SceneSolids, ConeLabelTellsTheTwoDriversOfADoubleCabinetApart)
{
    EXPECT_STRNE(ui::cone_label(SushiDSP::room::K_CABINET2X12, 0),
                 ui::cone_label(SushiDSP::room::K_CABINET2X12, 1));
}

TEST(Small_SceneSolids, FurnishingBoxStandsOnTheFloorAtItsPlace)
{
    SushiDSP::room::RoomScene::Furnishing bed{SushiDSP::room::FurnitureId::BedDouble, 1.0, true,
                                              3.0, 2.0};
    const SushiDSP::room::FurnitureItem& row = SushiDSP::room::furniture(bed.item);
    const ui::SceneBox box = ui::furnishing_box(bed);

    EXPECT_TRUE(near_geometry(box[0].z, 0.0));
    EXPECT_TRUE(near_geometry(box[4].z, static_cast<double>(row.box_m[2])));
}

TEST(Small_SceneSolids, FurnishingBoxSpansItsFootprintAboutItsPlace)
{
    SushiDSP::room::RoomScene::Furnishing bed{SushiDSP::room::FurnitureId::BedDouble, 1.0, true,
                                              3.0, 2.0};
    const SushiDSP::room::FurnitureItem& row = SushiDSP::room::furniture(bed.item);
    const ui::SceneBox box = ui::furnishing_box(bed);

    EXPECT_TRUE(corners_straddle(box, &ui::Point3::x, 3.0, 0.5 * static_cast<double>(row.box_m[0])));
    EXPECT_TRUE(corners_straddle(box, &ui::Point3::y, 2.0, 0.5 * static_cast<double>(row.box_m[1])));
}

TEST(Small_SceneSolids, NearestConeFindsTheConeAMicrophoneIsPointedAt)
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
    EXPECT_TRUE(near_geometry(aim->in_front_m, 0.05));
    EXPECT_TRUE(near_geometry(aim->off_axis_m, 0.02));
    EXPECT_TRUE(near_geometry(aim->distance_m, std::hypot(0.05, 0.02)));
}

TEST(Small_SceneSolids, NearestConeIsEmptyInARoomWithoutACabinet)
{
    const Arrangement arrangement;
    EXPECT_FALSE(ui::nearest_cone(arrangement, {1.0, 1.0, 1.0}).has_value());
}

namespace
{
    /// @brief An arrangement holding one cabinet, turned 0.7 rad, and the pose it stands at.
    struct TurnedCabinetScene
    {
            Arrangement arrangement;
            SourcePlacement source;

            TurnedCabinetScene()
            {
                std::string reason;
                source.id = 3;
                source.pose = Pose{{2.0, 2.0, 0.6}, 0.7};
                const bool added = arrangement.add_source(source, reason);
                EXPECT_TRUE(added) << reason;
            }

            /// @brief Where a microphone sits on a driver at the offsets the cases use.
            Pose microphone_on(std::uint32_t driver) const
            {
                return ui::microphone_on_cone(source.pose, source.cabinet, driver, 0.04, 0.03, -0.02);
            }
    };
} // namespace

class Small_SceneSolidsMicrophone : public ::testing::TestWithParam<DriverRow>
{
};

TEST_P(Small_SceneSolidsMicrophone, MicrophoneOnConeStandsWhereNearestConeThenReadsIt)
{
    const TurnedCabinetScene scene;
    const Pose placed = scene.microphone_on(GetParam().driver);

    const std::optional<ui::MicrophoneAim> aim =
        ui::nearest_cone(scene.arrangement, placed.position);

    ASSERT_TRUE(aim.has_value());
    EXPECT_EQ(aim->driver, GetParam().driver);
    EXPECT_TRUE(near_geometry(aim->in_front_m, 0.04));
    EXPECT_TRUE(near_geometry(aim->aside_m, 0.03));
    EXPECT_TRUE(near_geometry(aim->above_m, -0.02));
}

TEST_P(Small_SceneSolidsMicrophone, MicrophoneOnConeLooksBackAtTheBaffle)
{
    const TurnedCabinetScene scene;
    const Pose placed = scene.microphone_on(GetParam().driver);

    EXPECT_TRUE(near_geometry(std::cos(placed.yaw_rad - scene.source.pose.yaw_rad), -1.0));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_SceneSolidsMicrophone, ::testing::ValuesIn(four_drivers()),
                         driver_row_name);
