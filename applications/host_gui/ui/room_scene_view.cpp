/**************************************************************************/
/* room_scene_view.cpp                                                    */
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
 * @file room_scene_view.cpp
 * @brief Implements the room scene: the shell, the markers and the drags that move them.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/room_scene_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <SushiDSP/nodes/room/furniture.hpp>
#include "ui/scene_solids.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Holds a marker's radius, in pixels.
        constexpr float K_MARKER_RADIUS = 9.0f;

        /// @brief Holds how far from a marker's centre the pointer still picks it, in pixels.
        constexpr float K_PICK_RADIUS = 13.0f;

        /// @brief Holds the length of a marker's facing line, in metres.
        constexpr double K_FACING_LENGTH_M = 0.45;

        /// @brief Holds how far one pixel of a drag orbits the camera, in radians.
        constexpr double K_ORBIT_PER_PIXEL = 0.008;

        /// @brief Holds how much one wheel notch changes the camera's distance.
        constexpr double K_ZOOM_PER_NOTCH = 0.1;

        /// @brief Holds the room's largest side times this as the camera's first distance.
        constexpr double K_FRAMING = 1.5;

        /// @brief Returns a room pose's position as a scene point.
        Point3 point_of(const SushiDSP::room::Vec3& position)
        {
            return Point3{position.x, position.y, position.z};
        }

        /// @brief Returns a marker's fill by what it stands for.
        Color fill_of(SceneItemKind kind)
        {
            switch (kind)
            {
            case SceneItemKind::Cabinet:
                return rgba(214, 168, 74);
            case SceneItemKind::Microphone:
                return rgba(120, 200, 120);
            case SceneItemKind::Furnishing:
                return rgba(168, 146, 190);
            case SceneItemKind::Listener:
                break;
            }
            return rgba(96, 160, 220);
        }

        /// @brief Draws one room-space line when both of its ends are in front of the camera.
        void draw_line(ImDrawList* draw, const SceneCamera& camera, const Point3& from,
                       const Point3& to, ImU32 color, float thickness = 1.0f)
        {
            const SceneCamera::Projection a = camera.project(from);
            const SceneCamera::Projection b = camera.project(to);
            if (a.visible && b.visible)
            {
                draw->AddLine(ImVec2(a.screen.x, a.screen.y), ImVec2(b.screen.x, b.screen.y), color,
                              thickness);
            }
        }

        /// @brief Draws the floor's grid, one line a metre, and the room's twelve edges.
        void draw_shell(ImDrawList* draw, const SceneCamera& camera, const Point3& size)
        {
            const ImU32 grid = to_imu32(rgba(70, 70, 76));
            const ImU32 edge = to_imu32(palette().muted_text);
            for (double x = 0.0; x <= size.x + 1e-9; x += 1.0)
            {
                draw_line(draw, camera, Point3{x, 0.0, 0.0}, Point3{x, size.y, 0.0}, grid);
            }
            for (double y = 0.0; y <= size.y + 1e-9; y += 1.0)
            {
                draw_line(draw, camera, Point3{0.0, y, 0.0}, Point3{size.x, y, 0.0}, grid);
            }
            const double xs[2] = {0.0, size.x};
            const double ys[2] = {0.0, size.y};
            const double zs[2] = {0.0, size.z};
            for (int a = 0; a < 2; ++a)
            {
                for (int b = 0; b < 2; ++b)
                {
                    draw_line(draw, camera, Point3{0.0, ys[a], zs[b]}, Point3{size.x, ys[a], zs[b]},
                              edge);
                    draw_line(draw, camera, Point3{xs[a], 0.0, zs[b]}, Point3{xs[a], size.y, zs[b]},
                              edge);
                    draw_line(draw, camera, Point3{xs[a], ys[b], 0.0}, Point3{xs[a], ys[b], size.z},
                              edge);
                }
            }
        }

        /// @brief Draws a box's twelve edges.
        void draw_box(ImDrawList* draw, const SceneCamera& camera, const SceneBox& box, ImU32 color,
                      float thickness)
        {
            for (const std::array<std::uint8_t, 2>& edge : K_BOX_EDGES)
            {
                draw_line(draw, camera, box[edge[0]], box[edge[1]], color, thickness);
            }
        }

        /// @brief Draws one cabinet's box and the rim of each of its cones.
        void draw_cabinet(ImDrawList* draw, const SceneCamera& camera,
                          const SushiDSP::Room::SourcePlacement& source, bool selected)
        {
            const ImU32 color = to_imu32(fill_of(SceneItemKind::Cabinet));
            draw_box(draw, camera, cabinet_box(source.pose, source.cabinet), color,
                     selected ? 2.0f : 1.0f);
            for (std::uint32_t driver = 0; driver < source.cabinet.driver_count(); ++driver)
            {
                const std::array<Point3, K_CONE_OUTLINE_POINTS> rim =
                    cone_outline(source.pose, source.cabinet, driver);
                for (std::size_t point = 0; point < rim.size(); ++point)
                {
                    draw_line(draw, camera, rim[point], rim[(point + 1) % rim.size()], color, 1.5f);
                }
            }
        }

        /// @brief Draws one item: its stem down to the floor, its facing line, its marker and its
        /// label.
        void draw_item(ImDrawList* draw, const SceneCamera& camera, const SceneItem& item,
                       const char* label, bool selected)
        {
            const SceneCamera::Projection at = camera.project(item.position);
            if (!at.visible)
            {
                return;
            }
            const ImU32 fill = to_imu32(fill_of(item.kind));
            const Point3 floor{item.position.x, item.position.y, 0.0};
            draw_line(draw, camera, floor, item.position, to_imu32(rgba(110, 110, 118)));
            const Point3 facing{item.position.x + K_FACING_LENGTH_M * std::cos(item.yaw_rad),
                                item.position.y + K_FACING_LENGTH_M * std::sin(item.yaw_rad),
                                item.position.z};
            draw_line(draw, camera, item.position, facing, fill, 2.0f);

            const ImVec2 centre(at.screen.x, at.screen.y);
            draw->AddCircleFilled(centre, K_MARKER_RADIUS, fill);
            if (selected)
            {
                draw->AddCircle(centre, K_MARKER_RADIUS + 3.0f, to_imu32(palette().text), 0, 2.0f);
            }
            const ImVec2 text_size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(centre.x - text_size.x * 0.5f, centre.y - text_size.y * 0.5f),
                          to_imu32(rgba(20, 20, 24)), label);
        }

        /// @brief Writes a moved item's pose back into the arrangement.
        void write_back(SushiDSP::Room::Arrangement& arrangement, const SceneItem& item)
        {
            const SushiDSP::Room::Pose pose{{item.position.x, item.position.y, item.position.z},
                                            item.yaw_rad};
            switch (item.kind)
            {
            case SceneItemKind::Cabinet:
                arrangement.move_source(item.id, pose);
                break;
            case SceneItemKind::Microphone:
                if (const SushiDSP::Room::MicrophonePlacement* held =
                        arrangement.find_microphone(item.id))
                {
                    SushiDSP::Room::MicrophonePlacement moved = *held;
                    moved.pose = pose;
                    arrangement.update_microphone(moved);
                }
                break;
            case SceneItemKind::Listener:
                arrangement.set_listener(pose);
                break;
            case SceneItemKind::Furnishing:
            {
                SushiDSP::room::RoomScene shell = arrangement.shell();
                if (item.id < shell.furnishing_count)
                {
                    SushiDSP::room::RoomScene::Furnishing& entry = shell.furnishings[item.id];
                    entry.x_m = item.position.x;
                    entry.y_m = item.position.y;
                    entry.yaw_rad = item.yaw_rad;
                    arrangement.set_shell(shell);
                }
                break;
            }
            }
        }

        /// @brief Finds the item of a list that a remembered selection names.
        /// @return The item's index, or nullopt when the selection has left the room.
        std::optional<std::size_t> index_of(const std::vector<SceneItem>& items,
                                            const std::optional<SceneItem>& selected)
        {
            if (!selected.has_value())
            {
                return std::nullopt;
            }
            for (std::size_t index = 0; index < items.size(); ++index)
            {
                if (items[index].kind == selected->kind && items[index].id == selected->id)
                {
                    return index;
                }
            }
            return std::nullopt;
        }
    } // namespace

    std::vector<SceneItem> scene_items(const SushiDSP::Room::Arrangement& arrangement)
    {
        std::vector<SceneItem> items;
        for (const SushiDSP::Room::SourcePlacement& source : arrangement.sources())
        {
            items.push_back(SceneItem{SceneItemKind::Cabinet, source.id,
                                      point_of(source.pose.position), source.pose.yaw_rad});
        }
        for (const SushiDSP::Room::MicrophonePlacement& microphone : arrangement.microphones())
        {
            items.push_back(SceneItem{SceneItemKind::Microphone, microphone.id,
                                      point_of(microphone.pose.position),
                                      microphone.pose.yaw_rad});
        }
        items.push_back(SceneItem{SceneItemKind::Listener, 0u,
                                  point_of(arrangement.listener().position),
                                  arrangement.listener().yaw_rad});
        const SushiDSP::room::RoomScene& shell = arrangement.shell();
        for (std::uint32_t index = 0; index < shell.furnishing_count; ++index)
        {
            const SushiDSP::room::RoomScene::Furnishing& entry = shell.furnishings[index];
            if (!entry.placed)
            {
                continue;
            }
            // The marker sits at half the item's height, where its box is centred.
            const double height =
                static_cast<double>(SushiDSP::room::furniture(entry.item).box_m[2]);
            items.push_back(SceneItem{SceneItemKind::Furnishing, index,
                                      Point3{entry.x_m, entry.y_m, 0.5 * height}, entry.yaw_rad});
        }
        return items;
    }

    bool draw_room_scene(const char* id, SushiDSP::Room::Arrangement& arrangement,
                         RoomSceneState& state, ImVec2 size)
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        const bool pressed = ImGui::IsItemActivated();

        const Point3 dimensions = point_of(arrangement.shell().dimensions);
        const Point3 centre{0.5 * dimensions.x, 0.5 * dimensions.y, 0.3 * dimensions.z};
        state.camera.set_viewport(Vec2{origin.x, origin.y}, Vec2{size.x, size.y});
        if (!state.framed)
        {
            state.camera.look_at(centre, K_FRAMING * std::max(dimensions.x, dimensions.y));
            state.framed = true;
        }
        else
        {
            state.camera.look_at(centre, state.camera.distance());
        }

        std::vector<SceneItem> items = scene_items(arrangement);
        const ImGuiIO& io = ImGui::GetIO();
        const Vec2 pointer{io.MousePos.x, io.MousePos.y};
        if (pressed)
        {
            const std::optional<std::size_t> picked =
                pick_item(state.camera, items, pointer, K_PICK_RADIUS);
            state.dragging_item = picked.has_value();
            if (picked.has_value())
            {
                state.selected = items[*picked];
            }
        }
        if (!active)
        {
            state.dragging_item = false;
        }

        bool moved = false;
        const std::optional<std::size_t> selected = index_of(items, state.selected);
        const bool dragged = active && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);
        if (dragged && state.dragging_item && selected.has_value())
        {
            SceneItem& item = items[*selected];
            if (io.KeyShift)
            {
                // A furnishing stands on the floor and has no height to drag.
                if (item.kind != SceneItemKind::Furnishing)
                {
                    item.position = drag_height(state.camera, item, io.MouseDelta.y, dimensions);
                }
            }
            else if (io.KeyAlt)
            {
                item.yaw_rad = drag_turn(item, io.MouseDelta.x);
            }
            else
            {
                item.position = drag_across(state.camera, item, pointer, dimensions);
            }
            write_back(arrangement, item);
            state.selected = item;
            moved = true;
        }
        else if (dragged)
        {
            state.camera.orbit(-static_cast<double>(io.MouseDelta.x) * K_ORBIT_PER_PIXEL,
                               static_cast<double>(io.MouseDelta.y) * K_ORBIT_PER_PIXEL);
        }
        if (hovered && io.MouseWheel != 0.0f)
        {
            state.camera.zoom(1.0 - static_cast<double>(io.MouseWheel) * K_ZOOM_PER_NOTCH);
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
        draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                            to_imu32(palette().panel_background));
        draw_shell(draw, state.camera, dimensions);

        for (const SushiDSP::Room::SourcePlacement& source : arrangement.sources())
        {
            const bool chosen = state.selected.has_value() &&
                                state.selected->kind == SceneItemKind::Cabinet &&
                                state.selected->id == source.id;
            draw_cabinet(draw, state.camera, source, chosen);
        }
        const SushiDSP::room::RoomScene& shell = arrangement.shell();
        for (std::uint32_t index = 0; index < shell.furnishing_count; ++index)
        {
            if (shell.furnishings[index].placed)
            {
                const bool chosen = state.selected.has_value() &&
                                    state.selected->kind == SceneItemKind::Furnishing &&
                                    state.selected->id == index;
                draw_box(draw, state.camera, furnishing_box(shell.furnishings[index]),
                         to_imu32(fill_of(SceneItemKind::Furnishing)), chosen ? 2.0f : 1.0f);
            }
        }

        std::size_t cabinet = 0;
        std::size_t microphone = 0;
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            char label[8];
            switch (items[index].kind)
            {
            case SceneItemKind::Cabinet:
                std::snprintf(label, sizeof(label), "C%zu", ++cabinet);
                break;
            case SceneItemKind::Microphone:
                std::snprintf(label, sizeof(label), "M%zu", ++microphone);
                break;
            case SceneItemKind::Listener:
                std::snprintf(label, sizeof(label), "L");
                break;
            case SceneItemKind::Furnishing:
                std::snprintf(label, sizeof(label), "F%u",
                              static_cast<unsigned>(items[index].id + 1u));
                break;
            }
            draw_item(draw, state.camera, items[index], label, selected == index);
        }
        draw->AddText(ImVec2(origin.x + 8.0f, origin.y + size.y - 22.0f),
                      to_imu32(palette().muted_text),
                      "Drag a marker to move it. Shift: height. Alt: turn. Drag the room to orbit, "
                      "wheel to zoom.");
        draw->PopClipRect();
        return moved;
    }
} // namespace ui
