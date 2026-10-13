/**************************************************************************/
/* scene_camera.hpp                                                       */
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
 * @file scene_camera.hpp
 * @brief Declares the orbit camera the room scene is drawn through.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include "ui/geometry.hpp"

namespace ui
{
    /// @brief A point in room space, in metres, z up.
    struct Point3
    {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
    };

    /// @brief An orbit camera over a room: it circles a target at a distance and projects room
    /// points onto a screen rectangle in perspective. Pure arithmetic; no ImGui, no rig.
    ///
    /// The azimuth turns the camera about the vertical axis and the elevation lifts it above the
    /// floor. The camera never looks straight down or along the floor, so a floor point always has
    /// one screen point and a screen point always has one floor point in front of the camera.
    class SceneCamera
    {
        public:
            /// @brief Holds the lowest elevation the camera takes, in radians.
            static constexpr double K_MIN_ELEVATION = 0.10;

            /// @brief Holds the highest elevation the camera takes, in radians.
            static constexpr double K_MAX_ELEVATION = 1.50;

            /// @brief Holds the nearest the camera comes to its target, in metres.
            static constexpr double K_MIN_DISTANCE = 1.0;

            /// @brief Holds the farthest the camera stands from its target, in metres.
            static constexpr double K_MAX_DISTANCE = 60.0;

            /// @brief Holds the depth in front of the camera below which a point is not drawn.
            static constexpr double K_NEAR_PLANE = 0.05;

            /// @brief Holds where one room point lands on the screen.
            struct Projection
            {
                    Vec2 screen{0.0f, 0.0f}; ///< The screen point, in the viewport's pixels.
                    double depth = 0.0;      ///< The distance along the view axis, in metres.
                    bool visible = false;    ///< True when the point is in front of the near plane.
            };

            /// @brief Sets the screen rectangle the camera draws into.
            /// @param origin The rectangle's top-left corner, in pixels.
            /// @param size The rectangle's width and height, in pixels.
            void set_viewport(Vec2 origin, Vec2 size) noexcept;

            /// @brief Aims the camera at a point from a distance, keeping its angles.
            /// @param target The room point at the centre of the view.
            /// @param distance The distance to it, in metres; clamped to the camera's range.
            void look_at(const Point3& target, double distance) noexcept;

            /// @brief Sets both angles; the elevation is clamped to the camera's range.
            /// @param azimuth_rad The turn about the vertical axis, zero looking from +x.
            /// @param elevation_rad The lift above the floor.
            void set_angles(double azimuth_rad, double elevation_rad) noexcept;

            /// @brief Sets the vertical field of view.
            /// @param radians The angle from the top edge to the bottom edge of the viewport.
            void set_field_of_view(double radians) noexcept;

            /// @brief Turns the camera about its target; the elevation is clamped.
            /// @param azimuth_delta_rad The change of azimuth.
            /// @param elevation_delta_rad The change of elevation.
            void orbit(double azimuth_delta_rad, double elevation_delta_rad) noexcept;

            /// @brief Moves the camera toward or away from its target.
            /// @param factor Multiplies the distance; below 1 comes closer. The result is clamped.
            void zoom(double factor) noexcept;

            /// @brief Reads the camera's distance to its target.
            /// @return The distance in metres.
            double distance() const noexcept;

            /// @brief Projects a room point onto the screen.
            /// @param world The room point.
            /// @return The screen point, its depth and whether it is in front of the camera.
            Projection project(const Point3& world) const noexcept;

            /// @brief Finds the room point under a screen point on one horizontal plane.
            /// @param screen The screen point, in the viewport's pixels.
            /// @param height The plane's height above the floor, in metres.
            /// @param world_out Receives the room point.
            /// @return False when the ray through the screen point runs away from the plane.
            bool unproject_onto_plane(Vec2 screen, double height, Point3& world_out) const noexcept;

            /// @brief Returns how many metres one screen pixel spans at a room point's depth.
            /// @param world The room point.
            /// @return Metres per pixel, or 0 when the point is behind the near plane.
            double metres_per_pixel(const Point3& world) const noexcept;

        private:
            /// @brief Holds the camera's position and its three unit axes in room space.
            struct Frame
            {
                    Point3 eye;     ///< The camera's position.
                    Point3 forward; ///< The view axis, toward the target.
                    Point3 right;   ///< The screen's +x in room space.
                    Point3 up;      ///< The screen's up in room space.
            };

            /// @brief Works the frame out from the target, the angles and the distance.
            Frame frame() const noexcept;

            /// @brief Returns the focal length in pixels: half the viewport's height over
            /// tan(fov / 2).
            double focal_pixels() const noexcept;

            /// @brief Stores the room point at the centre of the view.
            Point3 target_{};

            /// @brief Stores the turn about the vertical axis, in radians.
            double azimuth_rad_ = -1.2;

            /// @brief Stores the lift above the floor, in radians.
            double elevation_rad_ = 0.55;

            /// @brief Stores the distance to the target, in metres.
            double distance_m_ = 8.0;

            /// @brief Stores the vertical field of view, in radians.
            double field_of_view_rad_ = 0.9;

            /// @brief Stores the viewport's top-left corner, in pixels.
            Vec2 origin_{0.0f, 0.0f};

            /// @brief Stores the viewport's size, in pixels.
            Vec2 size_{1.0f, 1.0f};
    };
} // namespace ui
