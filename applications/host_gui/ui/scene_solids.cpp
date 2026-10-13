/**************************************************************************/
/* scene_solids.cpp                                                       */
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
 * @file scene_solids.cpp
 * @brief Implements the room scene's boxes, cone outlines and the microphone's aim.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/scene_solids.hpp"

#include <cmath>

#include "SushiDSP/room/furniture.hpp"

namespace ui
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Returns a point moved along a cabinet's left and upward from another.
        Point3 on_baffle(const SushiDSP::Room::Pose& pose, double left_m, double up_m)
        {
            // The cabinet's left is its forward turned a quarter turn counter-clockwise.
            return Point3{pose.position.x - std::sin(pose.yaw_rad) * left_m,
                          pose.position.y + std::cos(pose.yaw_rad) * left_m,
                          pose.position.z + up_m};
        }
    } // namespace

    SceneBox oriented_box(const Point3& centre, double yaw_rad, double length_m, double width_m,
                          double height_m)
    {
        const double along_x = std::cos(yaw_rad);
        const double along_y = std::sin(yaw_rad);
        const double half_length = 0.5 * length_m;
        const double half_width = 0.5 * width_m;
        const double half_height = 0.5 * height_m;
        const double signs[4][2] = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}};
        SceneBox box{};
        for (std::size_t corner = 0; corner < 4; ++corner)
        {
            const double along = signs[corner][0] * half_length;
            const double across = signs[corner][1] * half_width;
            const double x = centre.x + along * along_x - across * along_y;
            const double y = centre.y + along * along_y + across * along_x;
            box[corner] = Point3{x, y, centre.z - half_height};
            box[corner + 4] = Point3{x, y, centre.z + half_height};
        }
        return box;
    }

    SceneBox cabinet_box(const SushiDSP::Room::Pose& pose,
                         const SushiDSP::room::CabinetGeometry& geometry)
    {
        const double back = 0.5 * geometry.depth_m;
        const Point3 centre{pose.position.x - std::cos(pose.yaw_rad) * back,
                            pose.position.y - std::sin(pose.yaw_rad) * back, pose.position.z};
        return oriented_box(centre, pose.yaw_rad, geometry.depth_m, geometry.baffle_width_m,
                            geometry.height_m);
    }

    Point3 cone_centre(const SushiDSP::Room::Pose& pose,
                       const SushiDSP::room::CabinetGeometry& geometry, std::uint32_t driver)
    {
        return on_baffle(pose, geometry.driver_left_m(driver), geometry.driver_up_m(driver));
    }

    std::array<Point3, K_CONE_OUTLINE_POINTS>
    cone_outline(const SushiDSP::Room::Pose& pose, const SushiDSP::room::CabinetGeometry& geometry,
                 std::uint32_t driver)
    {
        std::array<Point3, K_CONE_OUTLINE_POINTS> outline{};
        for (std::size_t point = 0; point < K_CONE_OUTLINE_POINTS; ++point)
        {
            const double angle =
                2.0 * K_PI * static_cast<double>(point) / static_cast<double>(K_CONE_OUTLINE_POINTS);
            outline[point] = on_baffle(
                pose, geometry.driver_left_m(driver) + geometry.cone_radius_m * std::cos(angle),
                geometry.driver_up_m(driver) + geometry.cone_radius_m * std::sin(angle));
        }
        return outline;
    }

    SceneBox furnishing_box(const SushiDSP::room::RoomScene::Furnishing& furnishing)
    {
        const SushiDSP::room::FurnitureItem& row = SushiDSP::room::furniture(furnishing.item);
        const double height = static_cast<double>(row.box_m[2]);
        return oriented_box(Point3{furnishing.x_m, furnishing.y_m, 0.5 * height},
                            furnishing.yaw_rad, static_cast<double>(row.box_m[0]),
                            static_cast<double>(row.box_m[1]), height);
    }

    std::optional<MicrophoneAim> nearest_cone(const SushiDSP::Room::Arrangement& arrangement,
                                              const SushiDSP::room::Vec3& position)
    {
        std::optional<MicrophoneAim> nearest;
        const auto& sources = arrangement.sources();
        for (std::size_t source = 0; source < sources.size(); ++source)
        {
            const SushiDSP::Room::Pose& pose = sources[source].pose;
            const SushiDSP::room::CabinetGeometry& geometry = sources[source].cabinet;
            const double forward_x = std::cos(pose.yaw_rad);
            const double forward_y = std::sin(pose.yaw_rad);
            for (std::uint32_t driver = 0; driver < geometry.driver_count(); ++driver)
            {
                const Point3 centre = cone_centre(pose, geometry, driver);
                const double dx = position.x - centre.x;
                const double dy = position.y - centre.y;
                const double dz = position.z - centre.z;
                const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (nearest.has_value() && distance >= nearest->distance_m)
                {
                    continue;
                }
                const double in_front = dx * forward_x + dy * forward_y;
                const double off_axis_squared = distance * distance - in_front * in_front;
                nearest = MicrophoneAim{source, driver, distance,
                                        std::sqrt(off_axis_squared > 0.0 ? off_axis_squared : 0.0),
                                        in_front};
            }
        }
        return nearest;
    }
} // namespace ui
