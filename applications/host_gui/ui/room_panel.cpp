/**************************************************************************/
/* room_panel.cpp                                                         */
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
 * @file room_panel.cpp
 * @brief Draws the room panel and writes its edits back through the controller.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/room_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include <SushiDSP/catalog/node_registry/node_registry.hpp>
#include <SushiDSP/nodes/room/furniture.hpp>
#include <SushiDSP/nodes/room/materials.hpp>
#include <SushiDSP/nodes/room/microphone_model.hpp>
#include <SushiDSP/nodes/room/modal_solver.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/room_scene.hpp>

#include "ui/furnishing_list.hpp"
#include "ui/patch_list.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        using SushiDSP::room::RoomScene;
        using SushiDSP::room::Vec3;

        /// @brief Holds the plan's inset from the window edge and between groups, in pixels.
        constexpr float K_PADDING = 8.0f;

        /// @brief Keeps a dragged pose this far inside the walls, in metres.
        constexpr double K_WALL_MARGIN = 0.05;

        /// @brief Holds the width of the rule under a section heading, in pixels.
        constexpr float K_SECTION_WIDTH = 720.0f;

        /// @brief Holds the width of a furnishing row's item combo, in pixels.
        constexpr float K_FURNISHING_ITEM_WIDTH = 400.0f;

        /// @brief Holds the width of a furnishing row's quantity field, in pixels.
        constexpr float K_FURNISHING_QUANTITY_WIDTH = 90.0f;

        /// @brief Holds the width of a patch row's surface combo, in pixels.
        constexpr float K_PATCH_SURFACE_WIDTH = 80.0f;

        /// @brief Holds the width of a patch row's material combo, in pixels.
        constexpr float K_PATCH_MATERIAL_WIDTH = 170.0f;

        /// @brief Holds the width of a patch row's area field, in pixels.
        constexpr float K_PATCH_AREA_WIDTH = 80.0f;

        /// @brief Names the six faces in Surface order, for the surface rows.
        constexpr const char* K_SURFACE_LABELS[SushiDSP::room::K_SURFACE_COUNT] = {
            "Wall -x", "Wall +x", "Wall -y", "Wall +y", "Floor", "Ceiling"};

        /// @brief Lists the scattering words for the combo, in Scattering order.
        constexpr const char* K_SCATTERING_ITEMS = "Flat\0Textured\0Cluttered\0";

        /// @brief Lists the direct-delay choices for the combo, in the parameter's order.
        constexpr const char* K_DIRECT_DELAY_ITEMS = "Compensated\0Physical\0";

        /// @brief Returns the face names joined for the combo, in Surface order.
        const std::string& surface_items()
        {
            static const std::string items = []
            {
                std::string joined;
                for (const char* label : K_SURFACE_LABELS)
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
                for (std::size_t index = 0; index < SushiDSP::room::K_FURNITURE_COUNT; ++index)
                {
                    joined +=
                        SushiDSP::room::furniture(static_cast<SushiDSP::room::FurnitureId>(index))
                            .name;
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
                for (std::size_t index = 0; index < SushiDSP::room::K_MATERIAL_COUNT; ++index)
                {
                    joined +=
                        SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(index))
                            .name;
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

        /// @brief Keeps one coordinate inside a wall pair, K_WALL_MARGIN off either wall.
        double clamp_inside(double value, double length)
        {
            return std::clamp(value, K_WALL_MARGIN,
                              std::max(K_WALL_MARGIN, length - K_WALL_MARGIN));
        }

        /// @brief Keeps a pose inside the shoebox after a dimension shrank under it.
        void clamp_pose(Vec3& position, const Vec3& dimensions)
        {
            position.x = clamp_inside(position.x, dimensions.x);
            position.y = clamp_inside(position.y, dimensions.y);
            position.z = clamp_inside(position.z, dimensions.z);
        }

        /// @brief Draws the three dimension fields and keeps every pose inside the new box.
        /// @return True on the frames a dimension changed.
        bool draw_dimensions(RoomScene& scene)
        {
            bool changed = false;
            ImGui::SetNextItemWidth(110.0f);
            changed =
                ImGui::InputDouble("Width x (m)", &scene.dimensions.x, 0.1, 1.0, "%.2f") || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed =
                ImGui::InputDouble("Depth y (m)", &scene.dimensions.y, 0.1, 1.0, "%.2f") || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("Height z (m)", &scene.dimensions.z, 0.1, 1.0, "%.2f") ||
                      changed;
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
                ImGui::SetNextItemWidth(K_PATCH_SURFACE_WIDTH);
                if (ImGui::Combo("##patch_surface", &surface, surface_items().c_str()))
                {
                    entry.surface = static_cast<SushiDSP::room::Surface>(surface);
                    changed = true;
                }
                ImGui::SameLine();
                int material = static_cast<int>(entry.material);
                ImGui::SetNextItemWidth(K_PATCH_MATERIAL_WIDTH);
                if (ImGui::Combo("##patch_material", &material, material_items().c_str()))
                {
                    entry.material = static_cast<SushiDSP::room::MaterialId>(material);
                    changed = true;
                }
                ImGui::SameLine();
                double area = entry.area_m2;
                ImGui::SetNextItemWidth(K_PATCH_AREA_WIDTH);
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
                                            static_cast<int>(SushiDSP::room::K_SURFACE_COUNT) - 1);
            int chosen_material =
                std::clamp(storage->GetInt(material_key, 0), 0,
                           static_cast<int>(SushiDSP::room::K_MATERIAL_COUNT) - 1);
            ImGui::SetNextItemWidth(K_PATCH_SURFACE_WIDTH);
            if (ImGui::Combo("##add_patch_surface", &chosen_surface, surface_items().c_str()))
            {
                storage->SetInt(surface_key, chosen_surface);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(K_PATCH_MATERIAL_WIDTH);
            if (ImGui::Combo("##add_patch_material", &chosen_material, material_items().c_str()))
            {
                storage->SetInt(material_key, chosen_material);
            }
            ImGui::SameLine();
            const bool full = scene.patch_count >= RoomScene::K_MAX_PATCHES;
            ImGui::BeginDisabled(full);
            if (ImGui::Button("Add"))
            {
                const SushiDSP::room::Surface surface =
                    static_cast<SushiDSP::room::Surface>(chosen_surface);
                changed = add_patch(scene, surface,
                                    static_cast<SushiDSP::room::MaterialId>(chosen_material),
                                    default_patch_area(surface)) ||
                          changed;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%u of %zu",
                               static_cast<unsigned>(scene.patch_count), RoomScene::K_MAX_PATCHES);
            return changed;
        }

        /// @brief Draws six rows of a material combo and a scattering combo.
        /// @return True on the frames a row changed.
        bool draw_surfaces(RoomScene& scene)
        {
            bool changed = false;
            for (std::size_t face = 0; face < SushiDSP::room::K_SURFACE_COUNT; ++face)
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
                if (ImGui::Combo("##scattering", &scattering, K_SCATTERING_ITEMS))
                {
                    scene.scattering[face] = static_cast<SushiDSP::room::Scattering>(scattering);
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(K_SURFACE_LABELS[face]);
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
                // The row is two lines, so a pane narrower than its fields still shows them all.
                ImGui::SetNextItemWidth(
                    std::min(K_FURNISHING_ITEM_WIDTH, ImGui::GetContentRegionAvail().x));
                if (ImGui::Combo("##item", &item, furniture_items().c_str()))
                {
                    entry.item = static_cast<SushiDSP::room::FurnitureId>(item);
                    changed = true;
                }
                bool placed = entry.placed;
                if (ImGui::Checkbox("In the room", &placed))
                {
                    changed = (placed ? place_furnishing(scene, index, 0.5 * scene.dimensions.x,
                                                         0.5 * scene.dimensions.y)
                                      : unplace_furnishing(scene, index)) ||
                              changed;
                }
                ImGui::SameLine();
                double quantity = entry.quantity;
                ImGui::SetNextItemWidth(K_FURNISHING_QUANTITY_WIDTH);
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
                                    static_cast<int>(SushiDSP::room::K_FURNITURE_COUNT) - 1);
            ImGui::SetNextItemWidth(
                std::min(K_FURNISHING_ITEM_WIDTH, ImGui::GetContentRegionAvail().x));
            if (ImGui::Combo("##add_item", &chosen, furniture_items().c_str()))
            {
                storage->SetInt(chosen_key, chosen);
            }
            const bool full = scene.furnishing_count >= RoomScene::K_MAX_FURNISHINGS;
            ImGui::BeginDisabled(full);
            if (ImGui::Button("Add"))
            {
                const SushiDSP::room::FurnitureId item =
                    static_cast<SushiDSP::room::FurnitureId>(chosen);
                if (add_furnishing(scene, item, default_furnishing_quantity(item)))
                {
                    place_furnishing(scene, scene.furnishing_count - 1u, 0.5 * scene.dimensions.x,
                                     0.5 * scene.dimensions.y);
                    changed = true;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%u of %zu",
                               static_cast<unsigned>(scene.furnishing_count),
                               RoomScene::K_MAX_FURNISHINGS);
            return changed;
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

        /// @brief Draws a section's name with a rule under it, K_SECTION_WIDTH wide.
        void draw_section_heading(const char* name)
        {
            ImGui::TextUnformatted(name);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(start, ImVec2(start.x + K_SECTION_WIDTH, start.y),
                                                to_imu32(palette().muted_text));
            ImGui::Dummy(ImVec2(K_SECTION_WIDTH, K_PADDING * 0.5f));
        }

        /// @brief Keeps the listener, every cabinet, microphone and placed furnishing inside the shell, after
        /// a dimension shrank under one of them.
        void keep_inside(SushiDSP::Room::Arrangement& arrangement)
        {
            const Vec3 dimensions = arrangement.shell().dimensions;
            SushiDSP::Room::Pose listener = arrangement.listener();
            clamp_pose(listener.position, dimensions);
            arrangement.set_listener(listener);
            for (SushiDSP::Room::SourcePlacement source : arrangement.sources())
            {
                clamp_pose(source.pose.position, dimensions);
                arrangement.move_source(source.id, source.pose);
            }
            for (SushiDSP::Room::MicrophonePlacement microphone : arrangement.microphones())
            {
                clamp_pose(microphone.pose.position, dimensions);
                arrangement.update_microphone(microphone);
            }
            RoomScene shell = arrangement.shell();
            for (std::uint32_t index = 0; index < shell.furnishing_count; ++index)
            {
                if (shell.furnishings[index].placed)
                {
                    place_furnishing(shell, index, shell.furnishings[index].x_m,
                                     shell.furnishings[index].y_m);
                }
            }
            arrangement.set_shell(shell);
        }

        /// @brief Draws the Schroeder frequency, Weyl's mode estimate under it and the HRTF
        /// source word for the room in place; a bypassed slot has no node to read.
        void draw_readouts(RigController& rig, RigController::SlotId slot, const RoomScene& scene)
        {
            SushiDSP::room::RoomHost* host = rig.room_host(slot);
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
    } // namespace

    void DrawRoomPanel(RigController& rig, RigController::SlotId slot, std::string& room_error)
    {
        ImGui::PushID(static_cast<int>(slot));
        SushiDSP::Room::Arrangement arrangement = rig.room_arrangement(slot);
        RoomScene scene = arrangement.shell();
        bool changed = false;

        ImGui::BeginGroup();
        draw_section_heading("Shell");
        changed = draw_dimensions(scene) || changed;
        ImGui::Spacing();
        ImGui::TextUnformatted("Openings and coverings");
        changed = draw_patches(scene) || changed;

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

        ImGui::Spacing();
        draw_section_heading("Direct sound");
        draw_parameter_choice(rig, slot, "direct_delay", K_DIRECT_DELAY_ITEMS);
        ImGui::Spacing();
        draw_readouts(rig, slot, scene);
        ImGui::EndGroup();

        if (changed)
        {
            arrangement.set_shell(scene);
            keep_inside(arrangement);
            std::string error;
            if (rig.set_room_arrangement(slot, arrangement, error))
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
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "Room refused: %s",
                               room_error.c_str());
        }
        ImGui::PopID();
    }
} // namespace ui
