/**************************************************************************/
/* test_scene_camera.cpp                                                  */
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
 * @file test_scene_camera.cpp
 * @brief Tests the scene camera's projection and the scene's picking and dragging against hand-computed coordinates.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"
#include "ui/scene_camera.hpp"
#include "ui/scene_interaction.hpp"

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The side of the square viewport every case uses, in pixels.
    constexpr double K_VIEWPORT_PIXELS = 200.0;

    /// @brief The focal length of that viewport at a 90 degree field of view, in pixels.
    constexpr double K_FOCAL_PIXELS = 100.0;

    /// @brief Rounded double operations on the longest path from the camera's angles to a
    /// projected coordinate: the frame's sines and cosines, the eye, the three unit vectors, the
    /// dot products, the perspective divide and the viewport offset.
    constexpr std::size_t K_PIPELINE_OPERATIONS = 40;

    /// @brief The bound, in pixels, on a projected screen coordinate against its hand-computed
    /// value: one float rounding of the coordinate plus the double pipeline's roundings at the
    /// viewport's scale.
    /// @param reference The hand-computed coordinate.
    double screen_bound(double reference)
    {
        return TestSupport::unit_roundoff<float>() * std::fabs(reference) +
               static_cast<double>(K_PIPELINE_OPERATIONS) * TestSupport::unit_roundoff<double>() *
                   K_VIEWPORT_PIXELS;
    }

    /// @brief The bound, in metres, on a world coordinate found from a screen coordinate.
    ///
    /// The screen coordinate carries a float rounding of at most u times the viewport's size; a
    /// pixel is depth over focal length metres across at that depth; and a ray meeting a plane at
    /// a grazing angle stretches the miss by the given factor, one over the sine of that angle.
    /// @param depth_m The largest depth the cases reach, in metres.
    /// @param grazing_amplification The largest one over the sine of the ray's angle to the plane.
    double world_bound(double depth_m, double grazing_amplification)
    {
        return grazing_amplification * (depth_m / K_FOCAL_PIXELS) * screen_bound(K_VIEWPORT_PIXELS);
    }

    /// @brief Checks a projected screen coordinate against its hand-computed value.
    ::testing::AssertionResult screen_is(float computed, double reference)
    {
        return TestSupport::is_within(
            TestSupport::measure_error(computed, reference, screen_bound(reference)));
    }

    /// @brief Checks a double against a reference within a bound.
    ::testing::AssertionResult within(double computed, double reference, double bound)
    {
        return TestSupport::is_within(TestSupport::measure_error(computed, reference, bound));
    }

    /// @brief Returns a camera on a 200 x 200 viewport with a 90 degree field of view, so the
    /// focal length is 100 pixels, looking at the origin from 2 m.
    ui::SceneCamera square_camera(double azimuth_rad, double elevation_rad)
    {
        ui::SceneCamera camera;
        camera.set_viewport(ui::Vec2{0.0f, 0.0f}, ui::Vec2{200.0f, 200.0f});
        camera.set_field_of_view(0.5 * K_PI);
        camera.set_angles(azimuth_rad, elevation_rad);
        camera.look_at(ui::Point3{0.0, 0.0, 0.0}, 2.0);
        return camera;
    }
} // namespace

TEST(Small_SceneCamera, SceneCameraProjectsTheTargetToTheCentreOfTheViewport)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.5);

    const ui::SceneCamera::Projection centre = camera.project(ui::Point3{0.0, 0.0, 0.0});

    ASSERT_TRUE(centre.visible);
    EXPECT_TRUE(screen_is(centre.screen.x, 100.0));
    EXPECT_TRUE(screen_is(centre.screen.y, 100.0));
}

TEST(Small_SceneCamera, SceneCameraPutsTheTargetAtItsDistanceInDepth)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.5);

    const ui::SceneCamera::Projection centre = camera.project(ui::Point3{0.0, 0.0, 0.0});

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(centre.depth, 2.0, K_PIPELINE_OPERATIONS)));
}

TEST(Small_SceneCamera, SceneCameraProjectsAPointToTheRightOfTheTarget)
{
    // From azimuth -pi/2 the right axis is +x: 1 m right at 2 m depth is 100 px / 2 = 50 px.
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    const ui::SceneCamera::Projection right = camera.project(ui::Point3{1.0, 0.0, 0.0});

    ASSERT_TRUE(right.visible);
    EXPECT_TRUE(screen_is(right.screen.x, 150.0));
    EXPECT_TRUE(screen_is(right.screen.y, 100.0));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(right.depth, 2.0, K_PIPELINE_OPERATIONS)));
}

TEST(Small_SceneCamera, SceneCameraProjectsAPointAboveTheTarget)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    // A point 1 m along the camera's up axis, (0, sin 0.1, cos 0.1), is 50 px above centre.
    const ui::SceneCamera::Projection above =
        camera.project(ui::Point3{0.0, std::sin(0.1), std::cos(0.1)});

    ASSERT_TRUE(above.visible);
    EXPECT_TRUE(screen_is(above.screen.x, 100.0));
    EXPECT_TRUE(screen_is(above.screen.y, 50.0));
}

TEST(Small_SceneCamera, SceneCameraHidesAPointBehindIt)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    // The camera stands near y = -2 and looks toward +y; y = -10 is behind it.
    EXPECT_FALSE(camera.project(ui::Point3{0.0, -10.0, 0.0}).visible);
}

TEST(Small_SceneCamera, SceneCameraFindsTheFloorPointUnderTheCentre)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);

    ui::Point3 floor;
    ASSERT_TRUE(camera.unproject_onto_plane(ui::Vec2{100.0f, 100.0f}, 0.0, floor));

    const double bound = world_bound(2.0, 2.0);
    EXPECT_TRUE(within(floor.x, 0.0, bound));
    EXPECT_TRUE(within(floor.y, 0.0, bound));
    EXPECT_EQ(floor.z, 0.0);
}

TEST(Small_SceneCamera, SceneCameraUnprojectsWhatItProjects)
{
    const ui::SceneCamera camera = square_camera(-1.1, 0.6);
    const ui::Point3 point{0.7, -0.4, 0.9};

    const ui::SceneCamera::Projection at = camera.project(point);
    ASSERT_TRUE(at.visible);
    ui::Point3 back;
    ASSERT_TRUE(camera.unproject_onto_plane(at.screen, point.z, back));

    const double bound = world_bound(3.0, 4.0);
    EXPECT_TRUE(within(back.x, point.x, bound));
    EXPECT_TRUE(within(back.y, point.y, bound));
}

TEST(Small_SceneCamera, SceneCameraRefusesAPlaneTheRayRunsAwayFrom)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);

    // The ray through the centre runs down, away from a plane 50 m above the floor.
    ui::Point3 unused;
    EXPECT_FALSE(camera.unproject_onto_plane(ui::Vec2{100.0f, 100.0f}, 50.0, unused));
}

TEST(Small_SceneCamera, SceneCameraClampsItsDistance)
{
    ui::SceneCamera camera = square_camera(0.0, 0.5);

    camera.zoom(1000.0);
    EXPECT_EQ(camera.distance(), ui::SceneCamera::K_MAX_DISTANCE);
    camera.zoom(1e-6);
    EXPECT_EQ(camera.distance(), ui::SceneCamera::K_MIN_DISTANCE);
}

TEST(Small_SceneCamera, SceneCameraKeepsTheTargetAtTheCentreAtItsHighestElevation)
{
    ui::SceneCamera camera = square_camera(0.0, 0.5);
    camera.orbit(0.0, 10.0);

    // Even at the highest elevation the target stays at the centre: the camera never tips over.
    const ui::SceneCamera::Projection centre = camera.project(ui::Point3{0.0, 0.0, 0.0});
    ASSERT_TRUE(centre.visible);
    EXPECT_TRUE(screen_is(centre.screen.x, 100.0));
    EXPECT_TRUE(screen_is(centre.screen.y, 100.0));
}

TEST(Small_SceneCamera, SceneCameraScalesPixelsToMetresByDepth)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    // At 2 m with a 100 px focal length one pixel spans 2 / 100 m.
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        camera.metres_per_pixel(ui::Point3{0.0, 0.0, 0.0}), 0.02, K_PIPELINE_OPERATIONS)));
}

TEST(Small_SceneCamera, SceneInteractionPicksTheMarkerUnderThePointer)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
    std::vector<ui::SceneItem> items(2);
    items[0].position = ui::Point3{0.0, 0.0, 0.0}; // lands at (100, 100)
    items[1].position = ui::Point3{1.0, 0.0, 0.0}; // lands at (150, 100)

    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{148.0f, 101.0f}, 10.0f), 1u);
    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{103.0f, 98.0f}, 10.0f), 0u);
    EXPECT_FALSE(ui::pick_item(camera, items, ui::Vec2{125.0f, 100.0f}, 10.0f).has_value());
}

TEST(Small_SceneCamera, SceneInteractionPicksTheNearerOfTwoMarkersOnOnePixel)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
    // Both lie on the view axis, so both land on the centre; the second is 1 m nearer.
    std::vector<ui::SceneItem> items(2);
    items[0].position = ui::Point3{0.0, 0.0, 0.0};
    items[1].position = ui::Point3{0.0, -std::cos(0.1), std::sin(0.1)};

    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{100.0f, 100.0f}, 10.0f), 1u);
}

namespace
{
    /// @brief The camera, room and item the drag-across cases share: a camera 6 m from the room's
    /// middle at 45 degrees, in a 6 x 4 x 2.5 m room, and an item standing at (3, 2, 0.5).
    struct DragAcrossScene
    {
            ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);
            ui::Point3 dimensions{6.0, 4.0, 2.5};
            ui::SceneItem item;

            DragAcrossScene()
            {
                camera.look_at(ui::Point3{3.0, 2.0, 0.0}, 6.0);
                item.position = ui::Point3{3.0, 2.0, 0.5};
            }

            /// @brief Drags the item to where a world point projects.
            ui::Point3 drag_to(const ui::Point3& wanted) const
            {
                return ui::drag_across(camera, item, camera.project(wanted).screen, dimensions);
            }
    };
} // namespace

TEST(Small_SceneCamera, SceneInteractionDragsAcrossTheRoomAtTheItemsHeight)
{
    const DragAcrossScene scene;

    // Dragging to where a point at the item's own height projects moves the item there.
    const ui::Point3 moved = scene.drag_to(ui::Point3{4.0, 1.5, 0.5});

    const double bound = world_bound(8.0, 2.0);
    EXPECT_TRUE(within(moved.x, 4.0, bound));
    EXPECT_TRUE(within(moved.y, 1.5, bound));
    EXPECT_EQ(moved.z, 0.5);
}

TEST(Small_SceneCamera, SceneInteractionKeepsADraggedItemInsideTheWallMargin)
{
    const DragAcrossScene scene;

    // A point outside the room is brought back to the wall margin.
    const ui::Point3 kept = scene.drag_to(ui::Point3{9.0, -3.0, 0.5});

    EXPECT_DOUBLE_EQ(kept.x, 6.0 - ui::K_SCENE_WALL_MARGIN);
    EXPECT_DOUBLE_EQ(kept.y, ui::K_SCENE_WALL_MARGIN);
}

namespace
{
    /// @brief The camera, room and item the drag-height cases share, with the item's depth worked
    /// out by hand.
    struct DragHeightScene
    {
            ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
            ui::Point3 dimensions{6.0, 4.0, 2.5};
            ui::SceneItem item;

            DragHeightScene()
            {
                item.position = ui::Point3{0.0, 0.0, 1.0};
            }

            /// @brief The item's depth in metres: the camera stands 2 m from the origin along
            /// (0, -cos 0.1, sin 0.1) and looks back at it, so the item at (0, 0, 1) is
            /// 2 cos^2 0.1 - sin 0.1 (1 - 2 sin 0.1) = 2 - sin 0.1 deep.
            static double item_depth()
            {
                return 2.0 - std::sin(0.1);
            }
    };
} // namespace

TEST(Small_SceneCamera, SceneInteractionRaisesAnItemByAnUpwardDragByThePixelSizeAtItsDepth)
{
    const DragHeightScene scene;

    // Ten pixels up, at one pixel per depth over focal length metres.
    const ui::Point3 raised = ui::drag_height(scene.camera, scene.item, -10.0f, scene.dimensions);

    const double expected = 1.0 + 10.0 * (DragHeightScene::item_depth() / K_FOCAL_PIXELS);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(
        raised.z, expected, K_PIPELINE_OPERATIONS)));
}

TEST(Small_SceneCamera, SceneInteractionKeepsAnItemRaisedByADragOverItsOwnSpot)
{
    const DragHeightScene scene;

    const ui::Point3 raised = ui::drag_height(scene.camera, scene.item, -10.0f, scene.dimensions);

    EXPECT_EQ(raised.x, 0.0);
    EXPECT_EQ(raised.y, 0.0);
}

TEST(Small_SceneCamera, SceneInteractionStopsARaisedItemAtTheCeilingMargin)
{
    const DragHeightScene scene;

    const ui::Point3 stopped =
        ui::drag_height(scene.camera, scene.item, -100000.0f, scene.dimensions);

    EXPECT_DOUBLE_EQ(stopped.z, 2.5 - ui::K_SCENE_WALL_MARGIN);
}

namespace
{
    /// @brief The roundings on the longest path of a yaw: the turn constant's product with the
    /// pixels, the sum with the yaw, and one subtraction of two pi, whose constant is rounded too.
    constexpr std::size_t K_YAW_OPERATIONS = 4;

    /// @brief The largest magnitude a yaw sum reaches in the cases: 3.2 plus two pi.
    constexpr double K_YAW_MAGNITUDE = 3.2 + 2.0 * K_PI;
} // namespace

TEST(Small_SceneCamera, SceneInteractionTurnsAnItemByAHalfRadianForFiftyPixels)
{
    ui::SceneItem item;
    item.yaw_rad = 0.0;

    const double bound =
        static_cast<double>(K_YAW_OPERATIONS) * TestSupport::unit_roundoff<double>() * K_YAW_MAGNITUDE;
    EXPECT_TRUE(within(ui::drag_turn(item, 50.0f), -0.5, bound));
}

TEST(Small_SceneCamera, SceneInteractionWrapsAYawThatTurnsPastPi)
{
    ui::SceneItem item;
    item.yaw_rad = 3.1;

    const double wrapped = ui::drag_turn(item, -10.0f);

    const double bound =
        static_cast<double>(K_YAW_OPERATIONS) * TestSupport::unit_roundoff<double>() * K_YAW_MAGNITUDE;
    EXPECT_TRUE(within(wrapped, 3.2 - 2.0 * K_PI, bound));
}
