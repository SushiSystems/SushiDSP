/**************************************************************************/
/* scene_interaction.hpp                                                  */
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
 * @file scene_interaction.hpp
 * @brief Declares picking and dragging in the room scene, as arithmetic on a camera and a list of items.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ui/geometry.hpp"
#include "ui/scene_camera.hpp"

namespace ui
{
    /// @brief What one marker of the room scene stands for.
    enum class SceneItemKind : std::uint8_t
    {
        Cabinet,
        Microphone,
        Listener
    };

    /// @brief One thing placed in the room, as the scene draws and moves it.
    struct SceneItem
    {
            SceneItemKind kind = SceneItemKind::Listener; ///< What the marker stands for.
            std::uint64_t id = 0;   ///< The arrangement's identity for it; unused for the listener.
            Point3 position{};      ///< Where it stands, in metres.
            double yaw_rad = 0.0;   ///< Which way it faces, zero facing +x.
    };

    /// @brief Holds how far inside the walls a moved item is kept, in metres.
    inline constexpr double K_SCENE_WALL_MARGIN = 0.05;

    /// @brief Holds how far one pixel of a turning drag turns an item, in radians.
    inline constexpr double K_SCENE_TURN_PER_PIXEL = 0.01;

    /// @brief Finds the item under a screen point: the one whose marker is nearest on the screen,
    /// within a radius, and nearer the camera when two markers are equally close.
    /// @param camera The camera the scene is drawn through.
    /// @param items The items in the scene.
    /// @param pointer The screen point.
    /// @param radius_pixels How far from a marker's centre a point still picks it.
    /// @return The item's index, or nullopt when no marker is that close.
    std::optional<std::size_t> pick_item(const SceneCamera& camera,
                                         const std::vector<SceneItem>& items, Vec2 pointer,
                                         float radius_pixels);

    /// @brief Moves an item across the room at its own height to the point under the pointer,
    /// kept inside the walls.
    /// @param camera The camera the scene is drawn through.
    /// @param item The item being dragged.
    /// @param pointer The screen point.
    /// @param dimensions The room's size, in metres.
    /// @return The item's new position; its old one when the pointer is over no part of the plane.
    Point3 drag_across(const SceneCamera& camera, const SceneItem& item, Vec2 pointer,
                       const Point3& dimensions);

    /// @brief Raises or lowers an item by a vertical drag, kept between the floor and the ceiling.
    /// @param camera The camera the scene is drawn through.
    /// @param item The item being dragged.
    /// @param pixels_down How far the pointer moved down the screen; negative is up.
    /// @param dimensions The room's size, in metres.
    /// @return The item's new position.
    Point3 drag_height(const SceneCamera& camera, const SceneItem& item, float pixels_down,
                       const Point3& dimensions);

    /// @brief Turns an item by a horizontal drag.
    /// @param item The item being turned.
    /// @param pixels_right How far the pointer moved to the right; negative is left.
    /// @return The item's new yaw, wrapped into (-pi, pi].
    double drag_turn(const SceneItem& item, float pixels_right);
} // namespace ui
