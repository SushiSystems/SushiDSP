/**************************************************************************/
/* scene_interaction.cpp                                                  */
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
 * @file scene_interaction.cpp
 * @brief Implements picking and dragging in the room scene.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/scene_interaction.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Holds pi.
        constexpr double K_PI = 3.14159265358979323846;

        /// @brief Holds how much nearer in pixels a marker must be to win over one in front of it.
        constexpr float K_TIE_PIXELS = 0.5f;

        /// @brief Keeps one coordinate inside a wall pair, the margin off either wall.
        double inside(double value, double span) noexcept
        {
            const double low = K_SCENE_WALL_MARGIN;
            const double high = std::max(low, span - K_SCENE_WALL_MARGIN);
            return std::clamp(value, low, high);
        }
    } // namespace

    std::optional<std::size_t> pick_item(const SceneCamera& camera,
                                         const std::vector<SceneItem>& items, Vec2 pointer,
                                         float radius_pixels)
    {
        std::optional<std::size_t> best;
        float best_distance = radius_pixels;
        double best_depth = 0.0;
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            const SceneCamera::Projection at = camera.project(items[index].position);
            if (!at.visible)
            {
                continue;
            }
            const float dx = at.screen.x - pointer.x;
            const float dy = at.screen.y - pointer.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance > radius_pixels)
            {
                continue;
            }
            const bool closer = distance < best_distance - K_TIE_PIXELS;
            const bool tied_and_in_front =
                std::fabs(distance - best_distance) <= K_TIE_PIXELS && at.depth < best_depth;
            if (!best.has_value() || closer || tied_and_in_front)
            {
                best = index;
                best_distance = distance;
                best_depth = at.depth;
            }
        }
        return best;
    }

    Point3 drag_across(const SceneCamera& camera, const SceneItem& item, Vec2 pointer,
                       const Point3& dimensions)
    {
        Point3 under;
        if (!camera.unproject_onto_plane(pointer, item.position.z, under))
        {
            return item.position;
        }
        return Point3{inside(under.x, dimensions.x), inside(under.y, dimensions.y),
                      item.position.z};
    }

    Point3 drag_height(const SceneCamera& camera, const SceneItem& item, float pixels_down,
                       const Point3& dimensions)
    {
        const double metres = -static_cast<double>(pixels_down) * camera.metres_per_pixel(item.position);
        return Point3{item.position.x, item.position.y,
                      inside(item.position.z + metres, dimensions.z)};
    }

    double drag_turn(const SceneItem& item, float pixels_right)
    {
        double yaw = item.yaw_rad - static_cast<double>(pixels_right) * K_SCENE_TURN_PER_PIXEL;
        while (yaw > K_PI)
        {
            yaw -= 2.0 * K_PI;
        }
        while (yaw <= -K_PI)
        {
            yaw += 2.0 * K_PI;
        }
        return yaw;
    }
} // namespace ui
