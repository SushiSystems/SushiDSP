/****************************************************************/
/* RoomPanel.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RoomPanel.cpp
 * @brief Draws the room panel and writes its edits back through the controller.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/RoomPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/room/Furniture.hpp"
#include "SushiDSP/room/Materials.hpp"
#include "SushiDSP/room/MicrophoneModel.hpp"
#include "SushiDSP/room/ModalSolver.hpp"
#include "SushiDSP/room/RoomNode.hpp"
#include "SushiDSP/room/RoomScene.hpp"

#include "ui/FurnishingList.hpp"
#include "ui/PatchList.hpp"
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

        /// @brief Holds the width of the rule under a section heading, in pixels.
        constexpr float kSectionWidth = 720.0f;

        /// @brief Holds the width of a furnishing row's item combo, in pixels.
        constexpr float kFurnishingItemWidth = 400.0f;

        /// @brief Holds the width of a furnishing row's quantity field, in pixels.
        constexpr float kFurnishingQuantityWidth = 90.0f;

        /// @brief Holds the width of a patch row's surface combo, in pixels.
        constexpr float kPatchSurfaceWidth = 80.0f;

        /// @brief Holds the width of a patch row's material combo, in pixels.
        constexpr float kPatchMaterialWidth = 170.0f;

        /// @brief Holds the width of a patch row's area field, in pixels.
        constexpr float kPatchAreaWidth = 80.0f;

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

        /// @brief Returns the face names joined for the combo, in Surface order.
        const std::string& surface_items()
        {
            static const std::string items = []
            {
                std::string joined;
                for (const char* label : kSurfaceLabels)
                {
                    joined += label;
                    joined.push_back('\0');
                }
                return joined;
            }();
            return items;
        }

        /// @brief Returns the furniture names joined for the combo, in FurnitureId order.
        const std::string& furniture_items()
        {
            static const std::string items = []
            {
                std::string joined;
                for (std::size_t index = 0; index < SushiDSP::room::kFurnitureCount; ++index)
                {
                    joined += SushiDSP::room::furniture(
                        static_cast<SushiDSP::room::FurnitureId>(index)).name;
                    joined.push_back('\0');
                }
                return joined;
            }();
            return items;
        }

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

        /// @brief Draws the patch list: a row per entry and the add row under it.
        /// @return True on the frames an entry was added, removed or changed.
        bool draw_patches(RoomScene& scene)
        {
            bool changed = false;
            std::uint32_t removed = scene.patch_count;
            for (std::uint32_t index = 0; index < scene.patch_count; ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                RoomScene::Patch& entry = scene.patches[index];

                int surface = static_cast<int>(entry.surface);
                ImGui::SetNextItemWidth(kPatchSurfaceWidth);
                if (ImGui::Combo("##patch_surface", &surface, surface_items().c_str()))
                {
                    entry.surface = static_cast<SushiDSP::room::Surface>(surface);
                    changed = true;
                }
                ImGui::SameLine();
                int material = static_cast<int>(entry.material);
                ImGui::SetNextItemWidth(kPatchMaterialWidth);
                if (ImGui::Combo("##patch_material", &material, material_items().c_str()))
                {
                    entry.material = static_cast<SushiDSP::room::MaterialId>(material);
                    changed = true;
                }
                ImGui::SameLine();
                double area = entry.area_m2;
                ImGui::SetNextItemWidth(kPatchAreaWidth);
                if (ImGui::InputDouble("##patch_area", &area, 0.1, 1.0, "%.2f m2"))
                {
                    changed = set_patch_area(scene, index, area) || changed;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                {
                    removed = index;
                }
                ImGui::PopID();
            }
            if (removed < scene.patch_count)
            {
                remove_patch(scene, removed);
                changed = true;
            }

            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID surface_key = ImGui::GetID("add_patch_surface");
            const ImGuiID material_key = ImGui::GetID("add_patch_material");
            int chosen_surface = std::clamp(storage->GetInt(surface_key, 0), 0,
                                            static_cast<int>(SushiDSP::room::kSurfaceCount) - 1);
            int chosen_material = std::clamp(storage->GetInt(material_key, 0), 0,
                                             static_cast<int>(SushiDSP::room::kMaterialCount) - 1);
            ImGui::SetNextItemWidth(kPatchSurfaceWidth);
            if (ImGui::Combo("##add_patch_surface", &chosen_surface, surface_items().c_str()))
            {
                storage->SetInt(surface_key, chosen_surface);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(kPatchMaterialWidth);
            if (ImGui::Combo("##add_patch_material", &chosen_material, material_items().c_str()))
            {
                storage->SetInt(material_key, chosen_material);
            }
            ImGui::SameLine();
            const bool full = scene.patch_count >= RoomScene::kMaxPatches;
            ImGui::BeginDisabled(full);
            if (ImGui::Button("Add"))
            {
                const SushiDSP::room::Surface surface =
                    static_cast<SushiDSP::room::Surface>(chosen_surface);
                changed = add_patch(scene, surface,
                                    static_cast<SushiDSP::room::MaterialId>(chosen_material),
                                    default_patch_area(surface)) || changed;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%u of %zu",
                               static_cast<unsigned>(scene.patch_count), RoomScene::kMaxPatches);
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

        /// @brief Draws the furnishing list: a row per entry and the add row under it.
        /// @return True on the frames an entry was added, removed or changed.
        bool draw_furnishings(RoomScene& scene)
        {
            bool changed = false;
            std::uint32_t removed = scene.furnishing_count;
            for (std::uint32_t index = 0; index < scene.furnishing_count; ++index)
            {
                ImGui::PushID(static_cast<int>(index));
                RoomScene::Furnishing& entry = scene.furnishings[index];

                int item = static_cast<int>(entry.item);
                ImGui::SetNextItemWidth(kFurnishingItemWidth);
                if (ImGui::Combo("##item", &item, furniture_items().c_str()))
                {
                    entry.item = static_cast<SushiDSP::room::FurnitureId>(item);
                    changed = true;
                }
                ImGui::SameLine();
                double quantity = entry.quantity;
                ImGui::SetNextItemWidth(kFurnishingQuantityWidth);
                if (ImGui::InputDouble("##quantity", &quantity, 0.5, 1.0, "%.2f"))
                {
                    changed = set_furnishing_quantity(scene, index, quantity) || changed;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                {
                    removed = index;
                }
                ImGui::PopID();
            }
            if (removed < scene.furnishing_count)
            {
                remove_furnishing(scene, removed);
                changed = true;
            }

            ImGuiStorage* storage = ImGui::GetStateStorage();
            const ImGuiID chosen_key = ImGui::GetID("add_item");
            int chosen = std::clamp(storage->GetInt(chosen_key, 0), 0,
                                    static_cast<int>(SushiDSP::room::kFurnitureCount) - 1);
            ImGui::SetNextItemWidth(kFurnishingItemWidth);
            if (ImGui::Combo("##add_item", &chosen, furniture_items().c_str()))
            {
                storage->SetInt(chosen_key, chosen);
            }
            ImGui::SameLine();
            const bool full = scene.furnishing_count >= RoomScene::kMaxFurnishings;
            ImGui::BeginDisabled(full);
            if (ImGui::Button("Add"))
            {
                const SushiDSP::room::FurnitureId item =
                    static_cast<SushiDSP::room::FurnitureId>(chosen);
                changed = add_furnishing(scene, item, default_furnishing_quantity(item)) || changed;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%u of %zu",
                               static_cast<unsigned>(scene.furnishing_count),
                               RoomScene::kMaxFurnishings);
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

        /// @brief Draws a section's name with a rule under it, kSectionWidth wide.
        void draw_section_heading(const char* name)
        {
            ImGui::TextUnformatted(name);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(start, ImVec2(start.x + kSectionWidth, start.y),
                                                to_imu32(palette().muted_text));
            ImGui::Dummy(ImVec2(kSectionWidth, kPadding * 0.5f));
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
        draw_section_heading("Shell");
        ImGui::BeginGroup();
        changed = draw_dimensions(scene) || changed;
        ImGui::Spacing();
        ImGui::TextUnformatted("Openings and coverings");
        changed = draw_patches(scene) || changed;
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

        ImGui::Spacing();
        draw_section_heading("Finishes");
        changed = draw_surfaces(scene) || changed;
        changed = ImGui::Checkbox("Built room loss", &scene.built_room_loss) || changed;
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Adds the absorption a real built room shows over its tabulated "
                              "surfaces; off for an ideal box or a sealed chamber.");
        }

        ImGui::Spacing();
        draw_section_heading("Furnishings");
        changed = draw_furnishings(scene) || changed;
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
