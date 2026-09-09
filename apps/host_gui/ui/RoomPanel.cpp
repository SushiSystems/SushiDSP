/**************************************************************************/
/* RoomPanel.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file RoomPanel.cpp
 * @brief Draws the room panel and writes its edits back through the controller.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/RoomPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/MicrophoneModel.hpp"
#include "SushiDSP/room/ModalSolver.hpp"
#include "SushiDSP/room/RoomNode.hpp"
#include "SushiDSP/room/RoomScene.hpp"

#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        using SushiDSP::room::RoomScene;
        using SushiDSP::room::Vec3;

        /// @brief Holds the plan's inset from the window edge and between groups, in pixels.
        constexpr float kPadding = 8.0f;

        /// @brief Holds a marker's radius in pixels.
        constexpr float kMarkerRadius = 8.0f;

        /// @brief Holds the plan's largest side in pixels.
        constexpr float kPlanSide = 260.0f;

        /// @brief Keeps a dragged pose this far inside the walls, in metres.
        constexpr double kWallMargin = 0.05;

        /// @brief Names the six faces in Surface order, for the surface rows.
        constexpr const char* kSurfaceLabels[SushiDSP::room::kSurfaceCount] = {
            "Wall -x", "Wall +x", "Wall -y", "Wall +y", "Floor", "Ceiling"
        };

        /// @brief Lists the scattering words for the combo, in Scattering order.
        constexpr const char* kScatteringItems = "Flat\0Textured\0Cluttered\0";

        /// @brief Lists the polar patterns for the combo, in PolarPattern order.
        constexpr const char* kPatternItems = "Omni\0Cardioid\0Supercardioid\0Figure eight\0";

        /// @brief Lists the listener choices for the combo, in the parameter's order.
        constexpr const char* kListenerItems = "Ears\0Microphone\0";

        /// @brief Lists the direct-delay choices for the combo, in the parameter's order.
        constexpr const char* kDirectDelayItems = "Compensated\0Physical\0";

        /// @brief Returns the material names joined for the combo, in MaterialId order.
        const std::string& material_items()
        {
            static const std::string items = []
            {
                std::string joined;
                for (std::size_t index = 0; index < SushiDSP::room::kMaterialCount; ++index)
                {
                    joined += SushiDSP::room::material(
                        static_cast<SushiDSP::room::MaterialId>(index)).name;
                    joined.push_back('\0');
                }
                return joined;
            }();
            return items;
        }

        /// @brief Finds one of the room node's parameters by id.
        const SushiDSP::ParameterDescriptor* room_parameter(const char* param_id)
        {
            const SushiDSP::NodeDescriptor& node = SushiDSP::room::room_descriptor();
            for (std::size_t p = 0; p < node.parameter_count; ++p)
            {
                if (std::strcmp(node.parameters[p].id, param_id) == 0)
                {
                    return &node.parameters[p];
                }
            }
            return nullptr;
        }

        /// @brief Returns the part of a path after its last separator.
        std::string file_name_of(const std::string& path)
        {
            const std::size_t cut = path.find_last_of("/\\");
            return cut == std::string::npos ? path : path.substr(cut + 1);
        }

        /// @brief Keeps one coordinate inside a wall pair, kWallMargin off either wall.
        double clamp_inside(double value, double length)
        {
            return std::clamp(value, kWallMargin, std::max(kWallMargin, length - kWallMargin));
        }

        /// @brief Keeps a pose inside the shoebox after a dimension shrank under it.
        void clamp_pose(Vec3& position, const Vec3& dimensions)
        {
            position.x = clamp_inside(position.x, dimensions.x);
            position.y = clamp_inside(position.y, dimensions.y);
            position.z = clamp_inside(position.z, dimensions.z);
        }

        /// @brief Maps the plan's screen rectangle onto the room's floor, y up the screen.
        struct PlanFrame
        {
            /// @brief Stores the plan's top-left corner in screen pixels.
            ImVec2 origin;

            /// @brief Stores the plan's size in screen pixels.
            ImVec2 size;

            /// @brief Stores the room's floor, in metres.
            Vec3 dimensions;

            /// @brief Returns the screen point of a floor position.
            ImVec2 to_screen(const Vec3& position) const
            {
                const float u = static_cast<float>(position.x / dimensions.x);
                const float v = static_cast<float>(position.y / dimensions.y);
                return ImVec2(origin.x + u * size.x, origin.y + (1.0f - v) * size.y);
            }

            /// @brief Returns the floor position of a screen point, kept inside the walls.
            void to_floor(ImVec2 point, Vec3& position) const
            {
                const double u = static_cast<double>((point.x - origin.x) / size.x);
                const double v = static_cast<double>(1.0f - (point.y - origin.y) / size.y);
                position.x = clamp_inside(u * dimensions.x, dimensions.x);
                position.y = clamp_inside(v * dimensions.y, dimensions.y);
            }
        };

        /// @brief Draws one draggable marker on the plan and moves the pose under a drag.
        /// @param id The ImGui id, unique within the plan.
        /// @param frame The plan's mapping.
        /// @param position In/out: the pose's floor position.
        /// @param yaw_rad The pose's facing, drawn as a short line.
        /// @param color The marker's fill.
        /// @param label The letter drawn on the marker.
        /// @return True on the frames the position moved.
        bool drag_marker(const char* id, const PlanFrame& frame, Vec3& position, double yaw_rad,
                         Color color, const char* label)
        {
            const ImVec2 centre = frame.to_screen(position);
            ImGui::SetCursorScreenPos(ImVec2(centre.x - kMarkerRadius, centre.y - kMarkerRadius));
            ImGui::InvisibleButton(id, ImVec2(2.0f * kMarkerRadius, 2.0f * kMarkerRadius));

            bool moved = false;
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                const Vec3 before = position;
                frame.to_floor(ImGui::GetMousePos(), position);
                moved = position.x != before.x || position.y != before.y;
            }

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 fill = to_imu32(color);
            const ImU32 ink = to_imu32(palette().text);
            draw->AddCircleFilled(centre, kMarkerRadius, fill);
            const float facing_x = static_cast<float>(std::cos(yaw_rad));
            const float facing_y = static_cast<float>(-std::sin(yaw_rad));
            draw->AddLine(centre,
                          ImVec2(centre.x + facing_x * kMarkerRadius * 2.0f,
                                 centre.y + facing_y * kMarkerRadius * 2.0f),
                          fill, 2.0f);
            const ImVec2 text_size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(centre.x - text_size.x * 0.5f, centre.y - text_size.y * 0.5f),
                          ink, label);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s at (%.2f, %.2f, %.2f) m", label, position.x, position.y,
                                  position.z);
            }
            return moved;
        }

        /// @brief Draws the plan: the floor's outline scaled to the room and the three markers.
        /// @return True on the frames a marker moved.
        bool draw_plan(RoomScene& scene)
        {
            const float scale = kPlanSide / static_cast<float>(
                std::max(scene.dimensions.x, scene.dimensions.y));
            PlanFrame frame;
            frame.origin = ImGui::GetCursorScreenPos();
            frame.origin.x += kMarkerRadius;
            frame.origin.y += kMarkerRadius;
            frame.size = ImVec2(static_cast<float>(scene.dimensions.x) * scale,
                                static_cast<float>(scene.dimensions.y) * scale);
            frame.dimensions = scene.dimensions;

            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(frame.origin,
                                ImVec2(frame.origin.x + frame.size.x, frame.origin.y + frame.size.y),
                                to_imu32(palette().panel_background));
            draw->AddRect(frame.origin,
                          ImVec2(frame.origin.x + frame.size.x, frame.origin.y + frame.size.y),
                          to_imu32(palette().muted_text));

            bool moved = false;
            moved = drag_marker("cabinet", frame, scene.source_position, scene.source_yaw_rad,
                                rgba(214, 168, 74), "C") || moved;
            moved = drag_marker("head", frame, scene.listener_position, scene.listener_yaw_rad,
                                rgba(96, 160, 220), "H") || moved;
            moved = drag_marker("microphone", frame, scene.microphone_position,
                                scene.microphone_yaw_rad, rgba(120, 200, 120), "M") || moved;

            ImGui::SetCursorScreenPos(ImVec2(frame.origin.x - kMarkerRadius,
                                             frame.origin.y + frame.size.y + kMarkerRadius));
            ImGui::Dummy(ImVec2(frame.size.x + 2.0f * kMarkerRadius, 1.0f));
            return moved;
        }

        /// @brief Draws the three dimension fields and keeps every pose inside the new box.
        /// @return True on the frames a dimension changed.
        bool draw_dimensions(RoomScene& scene)
        {
            bool changed = false;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("Width x (m)", &scene.dimensions.x, 0.1, 1.0, "%.2f")
                || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("Depth y (m)", &scene.dimensions.y, 0.1, 1.0, "%.2f")
                || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("Height z (m)", &scene.dimensions.z, 0.1, 1.0, "%.2f")
                || changed;
            if (changed)
            {
                clamp_pose(scene.source_position, scene.dimensions);
                clamp_pose(scene.listener_position, scene.dimensions);
                clamp_pose(scene.microphone_position, scene.dimensions);
            }
            return changed;
        }

        /// @brief Draws six rows of a material combo and a scattering combo.
        /// @return True on the frames a row changed.
        bool draw_surfaces(RoomScene& scene)
        {
            bool changed = false;
            for (std::size_t face = 0; face < SushiDSP::room::kSurfaceCount; ++face)
            {
                ImGui::PushID(static_cast<int>(face));
                int material = static_cast<int>(scene.surfaces[face]);
                int scattering = static_cast<int>(scene.scattering[face]);
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::Combo("##material", &material, material_items().c_str()))
                {
                    scene.surfaces[face] = static_cast<SushiDSP::room::MaterialId>(material);
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo("##scattering", &scattering, kScatteringItems))
                {
                    scene.scattering[face] = static_cast<SushiDSP::room::Scattering>(scattering);
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(kSurfaceLabels[face]);
                ImGui::PopID();
            }
            return changed;
        }

        /// @brief Draws one pose's height field and facing slider.
        /// @return True on the frames either changed.
        bool draw_pose_fields(const char* id, const char* name, Vec3& position, double& yaw_rad,
                              const Vec3& dimensions)
        {
            ImGui::PushID(id);
            bool changed = false;
            ImGui::TextUnformatted(name);
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::InputDouble("height (m)", &position.z, 0.05, 0.5, "%.2f"))
            {
                position.z = clamp_inside(position.z, dimensions.z);
                changed = true;
            }
            ImGui::SameLine();
            float yaw = static_cast<float>(yaw_rad);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderAngle("facing", &yaw, -180.0f, 180.0f))
            {
                yaw_rad = static_cast<double>(yaw);
                changed = true;
            }
            ImGui::PopID();
            return changed;
        }

        /// @brief Draws the microphone's pattern combo.
        /// @return True on the frames the pattern changed.
        bool draw_microphone(RoomScene& scene)
        {
            int pattern = static_cast<int>(scene.microphone_pattern);
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::Combo("Microphone pattern", &pattern, kPatternItems))
            {
                scene.microphone_pattern = static_cast<SushiDSP::room::PolarPattern>(pattern);
                return true;
            }
            return false;
        }

        /// @brief Draws one of the node's continuous parameters as a slider over its range.
        void draw_parameter_slider(RigController& rig, RigController::SlotId slot,
                                   const char* param_id, const char* format)
        {
            const SushiDSP::ParameterDescriptor* param = room_parameter(param_id);
            if (param == nullptr)
            {
                return;
            }
            float value = rig.get_parameter(slot, param_id);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat(param->label, &value, param->min, param->max, format))
            {
                rig.set_parameter(slot, param_id, value);
            }
        }

        /// @brief Draws one of the node's two-way choices as a combo.
        void draw_parameter_choice(RigController& rig, RigController::SlotId slot,
                                   const char* param_id, const char* items)
        {
            const SushiDSP::ParameterDescriptor* param = room_parameter(param_id);
            if (param == nullptr)
            {
                return;
            }
            int choice = rig.get_parameter(slot, param_id) >= 0.5f ? 1 : 0;
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::Combo(param->label, &choice, items))
            {
                rig.set_parameter(slot, param_id, choice == 1 ? 1.0f : 0.0f);
            }
        }

        /// @brief Draws the mix group: the node's four parameters.
        void draw_mix(RigController& rig, RigController::SlotId slot)
        {
            draw_parameter_slider(rig, slot, "mix", "%.2f");
            draw_parameter_slider(rig, slot, "level", "%.1f dB");
            draw_parameter_choice(rig, slot, "listener", kListenerItems);
            draw_parameter_choice(rig, slot, "direct_delay", kDirectDelayItems);
        }

        /// @brief Draws the Schroeder frequency, Weyl's mode estimate under it and the HRTF
        /// source word for the room in place; a bypassed slot has no node to read.
        void draw_readouts(RigController& rig, RigController::SlotId slot, const RoomScene& scene)
        {
            SushiDSP::room::IRoomHost* host = rig.room_host(slot);
            if (host == nullptr)
            {
                ImGui::TextColored(to_imvec4(palette().muted_text),
                                   "Bypassed: the room is not rendering.");
                return;
            }

            const double schroeder_hz = host->schroeder_frequency_hz();
            const double modes = SushiDSP::room::ModalSolver::estimated_count(scene, schroeder_hz);
            ImGui::Text("Schroeder frequency %.0f Hz, about %.0f modes under it", schroeder_hz,
                        std::max(0.0, modes));

            if (host->hrtf_source() == SushiDSP::room::HrtfSource::Measured)
            {
                ImGui::Text("HRTF: %s", file_name_of(rig.hrtf_path()).c_str());
            }
            else
            {
                ImGui::TextColored(to_imvec4(rgba(214, 168, 74)),
                                   "HRTF: spherical head - front and back sound alike");
            }
        }
    }

    void DrawRoomPanel(RigController& rig, RigController::SlotId slot, std::string& room_error)
    {
        ImGui::PushID(static_cast<int>(slot));
        RoomScene scene = rig.room_scene(slot);
        bool changed = false;

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Room");
        changed = draw_dimensions(scene) || changed;
        ImGui::Spacing();
        changed = draw_surfaces(scene) || changed;
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 3.0f * kPadding);

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Plan: drag the cabinet, the head and the microphone");
        changed = draw_plan(scene) || changed;
        changed = draw_pose_fields("cabinet", "Cabinet", scene.source_position,
                                   scene.source_yaw_rad, scene.dimensions) || changed;
        changed = draw_pose_fields("head", "Head", scene.listener_position,
                                   scene.listener_yaw_rad, scene.dimensions) || changed;
        changed = draw_pose_fields("microphone", "Microphone", scene.microphone_position,
                                   scene.microphone_yaw_rad, scene.dimensions) || changed;
        changed = draw_microphone(scene) || changed;
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 3.0f * kPadding);

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Mix");
        draw_mix(rig, slot);
        ImGui::Spacing();
        draw_readouts(rig, slot, scene);
        ImGui::EndGroup();

        if (changed)
        {
            std::string error;
            if (rig.set_room_scene(slot, scene, error))
            {
                room_error.clear();
            }
            else
            {
                room_error = error;
            }
        }
        if (!room_error.empty())
        {
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "Scene refused: %s",
                               room_error.c_str());
        }
        ImGui::PopID();
    }
}
