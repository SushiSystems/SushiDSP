/**************************************************************************/
/* scene_solids.hpp                                                       */
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
 * @file scene_solids.hpp
 * @brief Declares the boxes and cones the room scene draws and where a microphone stands to a cone.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "SushiDSP/room/cabinet_geometry.hpp"
#include "SushiDSP/room/room_arrangement.hpp"
#include "SushiDSP/room/room_scene.hpp"
#include "ui/scene_camera.hpp"

namespace ui
{
    /// @brief Holds a box's eight corners in room space: 0 to 3 round the bottom, 4 to 7 above
    /// them in the same order.
    using SceneBox = std::array<Point3, 8>;

    /// @brief Lists the corner pairs that are a box's twelve edges.
    inline constexpr std::array<std::array<std::uint8_t, 2>, 12> K_BOX_EDGES = {
        {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6},
         {3, 7}}};

    /// @brief Counts the points a cone's outline is drawn with.
    inline constexpr std::size_t K_CONE_OUTLINE_POINTS = 20;

    /// @brief Builds a box turned about the vertical.
    /// @param centre The box's centre.
    /// @param yaw_rad The direction its length runs along, zero along +x.
    /// @param length_m Its size along that direction.
    /// @param width_m Its size across it.
    /// @param height_m Its size upward.
    SceneBox oriented_box(const Point3& centre, double yaw_rad, double length_m, double width_m,
                          double height_m);

    /// @brief Builds a cabinet's box: the baffle is the face the pose stands on and the box runs
    /// back from it.
    /// @param pose The baffle's centre and facing.
    SceneBox cabinet_box(const SushiDSP::Room::Pose& pose,
                         const SushiDSP::room::CabinetGeometry& geometry);

    /// @brief Returns where one cone's centre stands on the baffle.
    /// @param driver The cone, below geometry.driver_count().
    Point3 cone_centre(const SushiDSP::Room::Pose& pose,
                       const SushiDSP::room::CabinetGeometry& geometry, std::uint32_t driver);

    /// @brief Returns the rim of one cone as points round its centre, in the baffle's plane.
    /// @param driver The cone, below geometry.driver_count().
    std::array<Point3, K_CONE_OUTLINE_POINTS>
    cone_outline(const SushiDSP::Room::Pose& pose, const SushiDSP::room::CabinetGeometry& geometry,
                 std::uint32_t driver);

    /// @brief Builds a placed furnishing's box, standing on the floor.
    SceneBox furnishing_box(const SushiDSP::room::RoomScene::Furnishing& furnishing);

    /// @brief Says where a microphone stands to the cone nearest it.
    struct MicrophoneAim
    {
            std::size_t source = 0;     ///< The cabinet's index in the arrangement.
            std::uint32_t driver = 0;   ///< The cone's index on that cabinet.
            double distance_m = 0.0;    ///< From the capsule to the cone's centre.
            double off_axis_m = 0.0;    ///< From the capsule to the cone's axis.
            double in_front_m = 0.0;    ///< From the baffle's plane; negative behind it.
    };

    /// @brief Finds the cone nearest a point among every cabinet in the room.
    /// @param arrangement The room.
    /// @param position The capsule's position.
    /// @return The aim, or nullopt when the room holds no cabinet.
    std::optional<MicrophoneAim> nearest_cone(const SushiDSP::Room::Arrangement& arrangement,
                                              const SushiDSP::room::Vec3& position);
} // namespace ui
