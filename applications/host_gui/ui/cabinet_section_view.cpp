/**************************************************************************/
/* cabinet_section_view.cpp                                               */
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
 * @file cabinet_section_view.cpp
 * @brief Draws the cabinet section's plan view, a top-down drawing rather than a perspective 3D
 * scene, and writes marker moves, yaw changes and microphone edits into the room layout.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/cabinet_section_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>

#include "SushiDSP/room/microphone_model.hpp"
#include "SushiDSP/room/room_scene.hpp"

#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        using SushiDSP::room::Vec3;

        /// @brief Holds a marker's half size in pixels.
        constexpr float K_MARKER_RADIUS = 8.0f;

        /// @brief Keeps a dragged marker this far inside the walls, in metres.
        constexpr double K_WALL_MARGIN = 0.05;

        /// @brief Holds the width of a yaw slider, in pixels.
        constexpr float K_YAW_WIDTH = 160.0f;

        /// @brief Names no marker in the drag key.
        constexpr int K_NO_MARKER = -1;

        /// @brief Names the listener in the drag key.
        constexpr int K_LISTENER_KEY = 0;

        /// @brief Offsets cabinet k's drag key: K_CABINET_KEY + k.
        constexpr int K_CABINET_KEY = 1;

        /// @brief Offsets microphone id's drag key: K_MICROPHONE_KEY + id.
        constexpr int K_MICROPHONE_KEY = 1000;

        /// @brief Names the polar patterns in PolarPattern order, for the microphone labels.
        constexpr const char* K_PATTERN_NAMES[] = {"omni", "cardioid", "supercardioid",
                                                   "figure eight"};

        /// @brief Keeps one coordinate inside a wall pair, K_WALL_MARGIN off either wall.
        double clamp_inside(double value, double length)
        {
            return std::clamp(value, K_WALL_MARGIN,
                              std::max(K_WALL_MARGIN, length - K_WALL_MARGIN));
        }

        /// @brief Maps the plan's screen rectangle onto the room's floor, y up the screen.
        struct PlanFrame
        {
                /// @brief Stores the plan's top-left corner in screen pixels.
                ImVec2 origin;

                /// @brief Stores the plan's size in screen pixels.
                ImVec2 size;

                /// @brief Stores the room's dimensions, in metres.
                Vec3 dimensions;

                /// @brief Returns the screen point of a floor position.
                ImVec2 to_screen(const Vec3& position) const
                {
                    const float u = static_cast<float>(position.x / dimensions.x);
                    const float v = static_cast<float>(position.y / dimensions.y);
                    return ImVec2(origin.x + u * size.x, origin.y + (1.0f - v) * size.y);
                }

                /// @brief Writes the floor position of a screen point, kept inside the walls.
                void to_floor(ImVec2 point, Vec3& position) const
                {
                    const double u = static_cast<double>((point.x - origin.x) / size.x);
                    const double v = static_cast<double>(1.0f - (point.y - origin.y) / size.y);
                    position.x = clamp_inside(u * dimensions.x, dimensions.x);
                    position.y = clamp_inside(v * dimensions.y, dimensions.y);
                }
        };

        /// @brief Builds the plan's frame at the cursor: the room's floor scaled to the height.
        PlanFrame make_frame(const Vec3& dimensions, float height)
        {
            const float scale = height / static_cast<float>(dimensions.y);
            PlanFrame frame;
            frame.origin = ImGui::GetCursorScreenPos();
            frame.origin.x += K_MARKER_RADIUS;
            frame.origin.y += K_MARKER_RADIUS;
            frame.size = ImVec2(static_cast<float>(dimensions.x) * scale, height);
            frame.dimensions = dimensions;
            return frame;
        }

        /// @brief Returns the screen point a facing line from a marker ends at.
        ImVec2 facing_point(ImVec2 centre, double yaw_rad, float length)
        {
            return ImVec2(centre.x + static_cast<float>(std::cos(yaw_rad)) * length,
                          centre.y - static_cast<float>(std::sin(yaw_rad)) * length);
        }

        /// @brief Draws a label centred under a marker.
        void draw_label(ImDrawList* draw, ImVec2 centre, const char* label)
        {
            const ImVec2 size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(centre.x - size.x * 0.5f, centre.y + K_MARKER_RADIUS),
                          to_imu32(palette().text), label);
        }

        /// @brief Draws the room's walls as the plan's outline.
        void draw_room(ImDrawList* draw, const PlanFrame& frame)
        {
            const ImVec2 corner(frame.origin.x + frame.size.x, frame.origin.y + frame.size.y);
            draw->AddRectFilled(frame.origin, corner, to_imu32(palette().panel_background));
            draw->AddRect(frame.origin, corner, to_imu32(palette().muted_text));
        }

        /// @brief Draws the listener as a triangle pointing along its facing.
        void draw_listener(ImDrawList* draw, const PlanFrame& frame, const RoomPlacement& place)
        {
            const ImVec2 centre = frame.to_screen(place.position);
            const double yaw = place.yaw_rad;
            const double back = 2.4;
            draw->AddTriangleFilled(facing_point(centre, yaw, K_MARKER_RADIUS * 1.5f),
                                    facing_point(centre, yaw + back, K_MARKER_RADIUS),
                                    facing_point(centre, yaw - back, K_MARKER_RADIUS),
                                    to_imu32(rgba(96, 160, 220)));
            draw_label(draw, centre, "Listener");
        }

        /// @brief Draws a cabinet as a square with a facing line, labelled with its path number.
        void draw_cabinet(ImDrawList* draw, const PlanFrame& frame, const RoomPlacement& place,
                          std::size_t path)
        {
            const ImVec2 centre = frame.to_screen(place.position);
            const ImU32 fill = to_imu32(rgba(214, 168, 74));
            draw->AddRectFilled(ImVec2(centre.x - K_MARKER_RADIUS, centre.y - K_MARKER_RADIUS),
                                ImVec2(centre.x + K_MARKER_RADIUS, centre.y + K_MARKER_RADIUS),
                                fill);
            draw->AddLine(centre, facing_point(centre, place.yaw_rad, K_MARKER_RADIUS * 2.0f),
                          fill, 2.0f);
            char label[24];
            std::snprintf(label, sizeof(label), "Path %zu", path + 1);
            draw_label(draw, centre, label);
        }

        /// @brief Returns a pattern's lower-case name.
        const char* pattern_name(SushiDSP::room::PolarPattern pattern)
        {
            const std::size_t index = static_cast<std::size_t>(pattern);
            return index < std::size(K_PATTERN_NAMES) ? K_PATTERN_NAMES[index] : "?";
        }

        /// @brief Draws a microphone as a circle with a facing line, labelled with id and pattern.
        void draw_microphone(ImDrawList* draw, const PlanFrame& frame, const MicrophoneSpot& spot)
        {
            const ImVec2 centre = frame.to_screen(spot.place.position);
            const ImU32 fill = to_imu32(rgba(120, 200, 120));
            draw->AddCircleFilled(centre, K_MARKER_RADIUS * 0.75f, fill);
            draw->AddLine(centre, facing_point(centre, spot.place.yaw_rad, K_MARKER_RADIUS * 2.0f),
                          fill, 2.0f);
            char label[48];
            std::snprintf(label, sizeof(label), "M%u %s", static_cast<unsigned>(spot.id),
                          pattern_name(spot.pattern));
            draw_label(draw, centre, label);
        }

        /// @brief Returns the squared screen distance from a point to a floor position.
        float screen_distance_sq(const PlanFrame& frame, ImVec2 point, const Vec3& position)
        {
            const ImVec2 centre = frame.to_screen(position);
            const float dx = centre.x - point.x;
            const float dy = centre.y - point.y;
            return dx * dx + dy * dy;
        }

        /// @brief Returns the drag key of the marker nearest a screen point.
        int nearest_marker(const RoomLayout& layout, const PlanFrame& frame, ImVec2 point)
        {
            int best = K_LISTENER_KEY;
            float best_distance = screen_distance_sq(frame, point, layout.listener().position);
            const std::vector<RoomPlacement>& cabinets = layout.cabinets();
            for (std::size_t k = 0; k < cabinets.size(); ++k)
            {
                const float distance = screen_distance_sq(frame, point, cabinets[k].position);
                if (distance < best_distance)
                {
                    best = K_CABINET_KEY + static_cast<int>(k);
                    best_distance = distance;
                }
            }
            for (const MicrophoneSpot& spot : layout.microphones())
            {
                const float distance = screen_distance_sq(frame, point, spot.place.position);
                if (distance < best_distance)
                {
                    best = K_MICROPHONE_KEY + static_cast<int>(spot.id);
                    best_distance = distance;
                }
            }
            return best;
        }

        /// @brief Moves the marker a drag key names to a screen point, inside the walls.
        /// @return True when the layout changed.
        bool move_marker(RoomLayout& layout, const PlanFrame& frame, int key, ImVec2 point)
        {
            if (key == K_LISTENER_KEY)
            {
                RoomPlacement place = layout.listener();
                frame.to_floor(point, place.position);
                layout.set_listener(place);
                return true;
            }
            if (key >= K_MICROPHONE_KEY)
            {
                const MicrophoneSpot* found =
                    layout.find_microphone(static_cast<std::uint32_t>(key - K_MICROPHONE_KEY));
                if (found == nullptr)
                {
                    return false;
                }
                MicrophoneSpot spot = *found;
                frame.to_floor(point, spot.place.position);
                return layout.update_microphone(spot);
            }
            const std::size_t index = static_cast<std::size_t>(key - K_CABINET_KEY);
            if (index >= layout.cabinets().size())
            {
                return false;
            }
            RoomPlacement place = layout.cabinets()[index];
            frame.to_floor(point, place.position);
            std::string reason;
            return layout.set_cabinet(index, place, reason);
        }

        /// @brief Takes the plan's mouse input: picks the nearest marker on a press and moves it
        /// while the left button drags.
        /// @return True on the frames a marker moved.
        bool handle_drag(RoomLayout& layout, const PlanFrame& frame)
        {
            ImGui::SetCursorScreenPos(frame.origin);
            ImGui::InvisibleButton("plan", frame.size);
            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID key_id = ImGui::GetID("dragged_marker");
            if (ImGui::IsItemActivated())
            {
                storage->SetInt(key_id, nearest_marker(layout, frame, ImGui::GetMousePos()));
            }
            if (!ImGui::IsItemActive())
            {
                storage->SetInt(key_id, K_NO_MARKER);
                return false;
            }
            const int key = storage->GetInt(key_id, K_NO_MARKER);
            if (key == K_NO_MARKER || !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                return false;
            }
            return move_marker(layout, frame, key, ImGui::GetMousePos());
        }

        /// @brief Draws the plan: the walls, the markers, and takes the drag.
        /// @return True on the frames a marker moved.
        bool draw_plan(RoomLayout& layout, float height)
        {
            const PlanFrame frame = make_frame(layout.shell().dimensions, height);
            const bool moved = handle_drag(layout, frame);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw_room(draw, frame);
            const std::vector<RoomPlacement>& cabinets = layout.cabinets();
            for (std::size_t k = 0; k < cabinets.size(); ++k)
            {
                draw_cabinet(draw, frame, cabinets[k], k);
            }
            for (const MicrophoneSpot& spot : layout.microphones())
            {
                draw_microphone(draw, frame, spot);
            }
            draw_listener(draw, frame, layout.listener());

            ImGui::SetCursorScreenPos(ImVec2(frame.origin.x - K_MARKER_RADIUS,
                                             frame.origin.y + frame.size.y + 2.0f * K_MARKER_RADIUS));
            ImGui::Dummy(ImVec2(frame.size.x + 2.0f * K_MARKER_RADIUS, 1.0f));
            return moved;
        }

        /// @brief Draws the add and remove microphone buttons; the remove targets the last one.
        /// @return True when a microphone was added or removed.
        bool draw_microphone_buttons(RoomLayout& layout)
        {
            bool changed = false;
            const bool full = layout.microphones().size() >= RoomLayout::K_MAX_MICROPHONES;
            ImGui::BeginDisabled(full);
            if (ImGui::Button("Add microphone"))
            {
                const Vec3& dimensions = layout.shell().dimensions;
                MicrophoneSpot spot;
                spot.place.position = Vec3{dimensions.x * 0.5, dimensions.y * 0.5,
                                           clamp_inside(1.2, dimensions.z)};
                std::string reason;
                changed = layout.add_microphone(spot, reason) != 0;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(layout.microphones().empty());
            if (ImGui::Button("Remove last microphone"))
            {
                changed = layout.remove_microphone(layout.microphones().back().id) || changed;
            }
            ImGui::EndDisabled();
            return changed;
        }

        /// @brief Draws one yaw slider over a facing in radians.
        /// @return True on the frame the facing changed.
        bool draw_yaw_slider(const char* label, double& yaw_rad)
        {
            float yaw = static_cast<float>(yaw_rad);
            ImGui::SetNextItemWidth(K_YAW_WIDTH);
            if (!ImGui::SliderAngle(label, &yaw, -180.0f, 180.0f))
            {
                return false;
            }
            yaw_rad = static_cast<double>(yaw);
            return true;
        }

        /// @brief Draws the listener's and each cabinet's yaw sliders.
        /// @return True when a facing changed.
        bool draw_placement_yaws(RoomLayout& layout)
        {
            bool changed = false;
            RoomPlacement listener = layout.listener();
            if (draw_yaw_slider("Listener facing", listener.yaw_rad))
            {
                layout.set_listener(listener);
                changed = true;
            }
            for (std::size_t k = 0; k < layout.cabinets().size(); ++k)
            {
                RoomPlacement place = layout.cabinets()[k];
                char label[32];
                std::snprintf(label, sizeof(label), "Path %zu facing", k + 1);
                std::string reason;
                if (draw_yaw_slider(label, place.yaw_rad))
                {
                    changed = layout.set_cabinet(k, place, reason) || changed;
                }
            }
            return changed;
        }

        /// @brief Draws each microphone's yaw slider.
        /// @return True when a facing changed.
        bool draw_microphone_yaws(RoomLayout& layout)
        {
            bool changed = false;
            const std::size_t count = layout.microphones().size();
            for (std::size_t m = 0; m < count; ++m)
            {
                MicrophoneSpot spot = layout.microphones()[m];
                char label[32];
                std::snprintf(label, sizeof(label), "M%u facing", static_cast<unsigned>(spot.id));
                if (draw_yaw_slider(label, spot.place.yaw_rad))
                {
                    changed = layout.update_microphone(spot) || changed;
                }
            }
            return changed;
        }
    } // namespace

    bool draw_cabinet_section_view(const char* id, RoomLayout& layout, float height)
    {
        ImGui::PushID(id);
        bool changed = draw_plan(layout, height);
        changed = draw_microphone_buttons(layout) || changed;
        changed = draw_placement_yaws(layout) || changed;
        changed = draw_microphone_yaws(layout) || changed;
        ImGui::PopID();
        return changed;
    }
} // namespace ui
