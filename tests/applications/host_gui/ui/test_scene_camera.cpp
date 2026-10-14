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
 * @file test_scene_camera.cpp
 * @brief Tests the scene camera's projection and the scene's picking and dragging against hand-computed coordinates.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "ui/scene_camera.hpp"
#include "ui/scene_interaction.hpp"

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

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

TEST(SceneCamera, SceneCameraProjectsTheTargetToTheCentreOfTheViewport)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.5);

    const ui::SceneCamera::Projection centre = camera.project(ui::Point3{0.0, 0.0, 0.0});

    ASSERT_TRUE(centre.visible);
    EXPECT_NEAR(centre.screen.x, 100.0f, 1e-3f);
    EXPECT_NEAR(centre.screen.y, 100.0f, 1e-3f);
    EXPECT_NEAR(centre.depth, 2.0, 1e-9);
}

TEST(SceneCamera, SceneCameraProjectsAgainstHandComputedCoordinates)
{
    // From azimuth -pi/2 the right axis is +x: 1 m right at 2 m depth is 100 px / 2 = 50 px.
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    const ui::SceneCamera::Projection right = camera.project(ui::Point3{1.0, 0.0, 0.0});

    ASSERT_TRUE(right.visible);
    EXPECT_NEAR(right.screen.x, 150.0f, 1e-3f);
    EXPECT_NEAR(right.screen.y, 100.0f, 1e-3f);
    EXPECT_NEAR(right.depth, 2.0, 1e-9);

    // A point 1 m along the camera's up axis, (0, sin 0.1, cos 0.1), is 50 px above centre.
    const ui::SceneCamera::Projection above =
        camera.project(ui::Point3{0.0, std::sin(0.1), std::cos(0.1)});
    ASSERT_TRUE(above.visible);
    EXPECT_NEAR(above.screen.x, 100.0f, 1e-3f);
    EXPECT_NEAR(above.screen.y, 50.0f, 1e-3f);
}

TEST(SceneCamera, SceneCameraHidesAPointBehindIt)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    // The camera stands near y = -2 and looks toward +y; y = -10 is behind it.
    EXPECT_FALSE(camera.project(ui::Point3{0.0, -10.0, 0.0}).visible);
}

TEST(SceneCamera, SceneCameraFindsTheFloorPointUnderTheCentre)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);

    ui::Point3 floor;
    ASSERT_TRUE(camera.unproject_onto_plane(ui::Vec2{100.0f, 100.0f}, 0.0, floor));

    EXPECT_NEAR(floor.x, 0.0, 1e-9);
    EXPECT_NEAR(floor.y, 0.0, 1e-9);
    EXPECT_EQ(floor.z, 0.0);
}

TEST(SceneCamera, SceneCameraUnprojectsWhatItProjects)
{
    const ui::SceneCamera camera = square_camera(-1.1, 0.6);
    const ui::Point3 point{0.7, -0.4, 0.9};

    const ui::SceneCamera::Projection at = camera.project(point);
    ASSERT_TRUE(at.visible);
    ui::Point3 back;
    ASSERT_TRUE(camera.unproject_onto_plane(at.screen, point.z, back));

    EXPECT_NEAR(back.x, point.x, 1e-4);
    EXPECT_NEAR(back.y, point.y, 1e-4);
}

TEST(SceneCamera, SceneCameraRefusesAPlaneTheRayRunsAwayFrom)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);

    // The ray through the centre runs down, away from a plane 50 m above the floor.
    ui::Point3 unused;
    EXPECT_FALSE(camera.unproject_onto_plane(ui::Vec2{100.0f, 100.0f}, 50.0, unused));
}

TEST(SceneCamera, SceneCameraClampsItsElevationAndItsDistance)
{
    ui::SceneCamera camera = square_camera(0.0, 0.5);

    camera.orbit(0.0, 10.0);
    camera.zoom(1000.0);
    EXPECT_EQ(camera.distance(), ui::SceneCamera::K_MAX_DISTANCE);
    camera.zoom(1e-6);
    EXPECT_EQ(camera.distance(), ui::SceneCamera::K_MIN_DISTANCE);

    // Even at the highest elevation the target stays at the centre: the camera never tips over.
    const ui::SceneCamera::Projection centre = camera.project(ui::Point3{0.0, 0.0, 0.0});
    ASSERT_TRUE(centre.visible);
    EXPECT_NEAR(centre.screen.x, 100.0f, 1e-3f);
    EXPECT_NEAR(centre.screen.y, 100.0f, 1e-3f);
}

TEST(SceneCamera, SceneCameraScalesPixelsToMetresByDepth)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);

    // At 2 m with a 100 px focal length one pixel spans 2 / 100 m.
    EXPECT_NEAR(camera.metres_per_pixel(ui::Point3{0.0, 0.0, 0.0}), 0.02, 1e-9);
}

TEST(SceneCamera, SceneInteractionPicksTheMarkerUnderThePointer)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
    std::vector<ui::SceneItem> items(2);
    items[0].position = ui::Point3{0.0, 0.0, 0.0}; // lands at (100, 100)
    items[1].position = ui::Point3{1.0, 0.0, 0.0}; // lands at (150, 100)

    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{148.0f, 101.0f}, 10.0f), 1u);
    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{103.0f, 98.0f}, 10.0f), 0u);
    EXPECT_FALSE(ui::pick_item(camera, items, ui::Vec2{125.0f, 100.0f}, 10.0f).has_value());
}

TEST(SceneCamera, SceneInteractionPicksTheNearerOfTwoMarkersOnOnePixel)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
    // Both lie on the view axis, so both land on the centre; the second is 1 m nearer.
    std::vector<ui::SceneItem> items(2);
    items[0].position = ui::Point3{0.0, 0.0, 0.0};
    items[1].position = ui::Point3{0.0, -std::cos(0.1), std::sin(0.1)};

    EXPECT_EQ(ui::pick_item(camera, items, ui::Vec2{100.0f, 100.0f}, 10.0f), 1u);
}

TEST(SceneCamera, SceneInteractionDragsAcrossTheRoomAtTheItemsHeightAndStaysInside)
{
    ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.25 * K_PI);
    camera.look_at(ui::Point3{3.0, 2.0, 0.0}, 6.0);
    const ui::Point3 dimensions{6.0, 4.0, 2.5};
    ui::SceneItem item;
    item.position = ui::Point3{3.0, 2.0, 0.5};

    // Dragging to where a point at the item's own height projects moves the item there.
    const ui::Point3 wanted{4.0, 1.5, 0.5};
    const ui::Point3 moved =
        ui::drag_across(camera, item, camera.project(wanted).screen, dimensions);
    EXPECT_NEAR(moved.x, 4.0, 1e-3);
    EXPECT_NEAR(moved.y, 1.5, 1e-3);
    EXPECT_EQ(moved.z, 0.5);

    // A point outside the room is brought back to the wall margin.
    const ui::Point3 outside{9.0, -3.0, 0.5};
    const ui::Point3 kept =
        ui::drag_across(camera, item, camera.project(outside).screen, dimensions);
    EXPECT_NEAR(kept.x, 6.0 - ui::K_SCENE_WALL_MARGIN, 1e-9);
    EXPECT_NEAR(kept.y, ui::K_SCENE_WALL_MARGIN, 1e-9);
}

TEST(SceneCamera, SceneInteractionRaisesAnItemByAnUpwardDragAndStopsAtTheCeiling)
{
    const ui::SceneCamera camera = square_camera(-0.5 * K_PI, 0.1);
    const ui::Point3 dimensions{6.0, 4.0, 2.5};
    ui::SceneItem item;
    item.position = ui::Point3{0.0, 0.0, 1.0};

    // 10 px up at 0.02 m per pixel... the item is not at the target, so read the scale it uses.
    const double scale = camera.metres_per_pixel(item.position);
    const ui::Point3 raised = ui::drag_height(camera, item, -10.0f, dimensions);
    EXPECT_NEAR(raised.z, 1.0 + 10.0 * scale, 1e-9);
    EXPECT_EQ(raised.x, 0.0);

    const ui::Point3 stopped = ui::drag_height(camera, item, -100000.0f, dimensions);
    EXPECT_NEAR(stopped.z, 2.5 - ui::K_SCENE_WALL_MARGIN, 1e-9);
}

TEST(SceneCamera, SceneInteractionTurnsAnItemAndWrapsItsYaw)
{
    ui::SceneItem item;
    item.yaw_rad = 0.0;
    EXPECT_NEAR(ui::drag_turn(item, 50.0f), -0.5, 1e-9);

    item.yaw_rad = 3.1;
    const double wrapped = ui::drag_turn(item, -10.0f);
    EXPECT_NEAR(wrapped, 3.2 - 2.0 * K_PI, 1e-9);
}
