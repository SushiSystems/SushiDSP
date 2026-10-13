/**************************************************************************/
/* scene_camera.cpp                                                       */
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
 * @file scene_camera.cpp
 * @brief Implements the orbit camera's projection and its inverse onto a horizontal plane.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/scene_camera.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Returns the dot product of two room vectors.
        double dot(const Point3& a, const Point3& b) noexcept
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        /// @brief Returns the cross product of two room vectors.
        Point3 cross(const Point3& a, const Point3& b) noexcept
        {
            return Point3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }

        /// @brief Returns a - b.
        Point3 minus(const Point3& a, const Point3& b) noexcept
        {
            return Point3{a.x - b.x, a.y - b.y, a.z - b.z};
        }

        /// @brief Returns a vector scaled to unit length; a zero vector is returned as it is.
        Point3 unit(const Point3& v) noexcept
        {
            const double length = std::sqrt(dot(v, v));
            return length > 0.0 ? Point3{v.x / length, v.y / length, v.z / length} : v;
        }
    } // namespace

    void SceneCamera::set_viewport(Vec2 origin, Vec2 size) noexcept
    {
        origin_ = origin;
        size_ = Vec2{std::max(size.x, 1.0f), std::max(size.y, 1.0f)};
    }

    void SceneCamera::look_at(const Point3& target, double distance) noexcept
    {
        target_ = target;
        distance_m_ = std::clamp(distance, K_MIN_DISTANCE, K_MAX_DISTANCE);
    }

    void SceneCamera::set_angles(double azimuth_rad, double elevation_rad) noexcept
    {
        azimuth_rad_ = azimuth_rad;
        elevation_rad_ = std::clamp(elevation_rad, K_MIN_ELEVATION, K_MAX_ELEVATION);
    }

    void SceneCamera::set_field_of_view(double radians) noexcept
    {
        field_of_view_rad_ = std::clamp(radians, 0.2, 2.4);
    }

    void SceneCamera::orbit(double azimuth_delta_rad, double elevation_delta_rad) noexcept
    {
        set_angles(azimuth_rad_ + azimuth_delta_rad, elevation_rad_ + elevation_delta_rad);
    }

    void SceneCamera::zoom(double factor) noexcept
    {
        distance_m_ = std::clamp(distance_m_ * factor, K_MIN_DISTANCE, K_MAX_DISTANCE);
    }

    double SceneCamera::distance() const noexcept
    {
        return distance_m_;
    }

    SceneCamera::Frame SceneCamera::frame() const noexcept
    {
        const double flat = std::cos(elevation_rad_);
        Frame result;
        result.eye = Point3{target_.x + distance_m_ * flat * std::cos(azimuth_rad_),
                            target_.y + distance_m_ * flat * std::sin(azimuth_rad_),
                            target_.z + distance_m_ * std::sin(elevation_rad_)};
        result.forward = unit(minus(target_, result.eye));
        result.right = unit(cross(result.forward, Point3{0.0, 0.0, 1.0}));
        result.up = cross(result.right, result.forward);
        return result;
    }

    double SceneCamera::focal_pixels() const noexcept
    {
        return 0.5 * static_cast<double>(size_.y) / std::tan(0.5 * field_of_view_rad_);
    }

    SceneCamera::Projection SceneCamera::project(const Point3& world) const noexcept
    {
        const Frame view = frame();
        const Point3 offset = minus(world, view.eye);
        Projection result;
        result.depth = dot(offset, view.forward);
        result.visible = result.depth > K_NEAR_PLANE;
        if (!result.visible)
        {
            return result;
        }
        const double scale = focal_pixels() / result.depth;
        result.screen = Vec2{static_cast<float>(origin_.x + 0.5 * size_.x +
                                                dot(offset, view.right) * scale),
                             static_cast<float>(origin_.y + 0.5 * size_.y -
                                                dot(offset, view.up) * scale)};
        return result;
    }

    bool SceneCamera::unproject_onto_plane(Vec2 screen, double height,
                                           Point3& world_out) const noexcept
    {
        const Frame view = frame();
        const double across = static_cast<double>(screen.x) - (origin_.x + 0.5 * size_.x);
        const double upward = (origin_.y + 0.5 * size_.y) - static_cast<double>(screen.y);
        const double focal = focal_pixels();
        const Point3 ray{view.forward.x * focal + view.right.x * across + view.up.x * upward,
                         view.forward.y * focal + view.right.y * across + view.up.y * upward,
                         view.forward.z * focal + view.right.z * across + view.up.z * upward};
        if (ray.z == 0.0)
        {
            return false;
        }
        const double along = (height - view.eye.z) / ray.z;
        if (along <= 0.0)
        {
            return false;
        }
        world_out = Point3{view.eye.x + along * ray.x, view.eye.y + along * ray.y, height};
        return true;
    }

    double SceneCamera::metres_per_pixel(const Point3& world) const noexcept
    {
        const Projection at = project(world);
        return at.visible ? at.depth / focal_pixels() : 0.0;
    }
} // namespace ui
