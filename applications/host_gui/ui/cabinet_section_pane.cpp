/**************************************************************************/
/* cabinet_section_pane.cpp                                               */
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
 * @file cabinet_section_pane.cpp
 * @brief Implements the cabinet section's pane.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/cabinet_section_pane.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>

#include <imgui.h>

#include "SushiDSP/room/room_arrangement.hpp"

#include "ui/mixer_panel.hpp"
#include "SushiDSP/room/cabinet_geometry.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "ui/room_panel.hpp"
#include "ui/scene_solids.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief Holds the inspector's width, in pixels.
        constexpr float K_INSPECTOR_WIDTH = 330.0f;

        /// @brief Holds the mixer's height, in pixels.
        constexpr float K_MIXER_HEIGHT = 330.0f;

        /// @brief Holds the least height the scene is given, in pixels.
        constexpr float K_MIN_SCENE_HEIGHT = 160.0f;

        /// @brief Lists the polar patterns for the combo, in the microphone's parameter order.
        constexpr const char* K_PATTERN_ITEMS = "Omni\0Cardioid\0Supercardioid\0Figure eight\0";

        /// @brief Lists the shipped cabinets for the combo, in CabinetKind order.
        constexpr const char* K_CABINET_ITEMS = "1x12\0" "2x12\0" "4x12\0";

        /// @brief Lists the DI positions for the combo, in DiPosition order.
        constexpr const char* K_DI_POSITION_ITEMS = "Before the effects\0After the effects\0";

        /// @brief Runs an edit that may be refused and keeps the refusal's reason.
        void attempt(std::string& error, const std::function<void()>& edit)
        {
            try
            {
                edit();
                error.clear();
            }
            catch (const std::exception& refusal)
            {
                error = refusal.what();
            }
        }

        /// @brief Draws the row above the scene: the microphone button and the DI path's switches.
        void draw_toolbar(RigController& rig, RigTopology& topology, TopologyApplier& applier,
                          CabinetSectionState& state)
        {
            if (ImGui::Button("Add microphone"))
            {
                attempt(state.error, [&]
                        {
                            const SlotId added = applier.add_microphone(topology, rig);
                            state.scene.selected =
                                SceneItem{SceneItemKind::Microphone, added, Point3{}, 0.0};
                        });
            }
            ImGui::SameLine(0.0f, 24.0f);
            bool di = topology.di();
            if (ImGui::Checkbox("DI path", &di))
            {
                attempt(state.error, [&]
                        {
                            applier.set_di(topology, rig, di);
                        });
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!topology.di());
            int position = topology.di_position() == DiPosition::BeforeEffects ? 0 : 1;
            ImGui::SetNextItemWidth(170.0f);
            if (ImGui::Combo("##di_position", &position, K_DI_POSITION_ITEMS))
            {
                attempt(state.error, [&]
                        {
                            applier.set_di_position(topology, rig,
                                                    position == 0 ? DiPosition::BeforeEffects
                                                                  : DiPosition::AfterEffects);
                        });
            }
            ImGui::EndDisabled();
            if (!state.error.empty())
            {
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "Refused: %s",
                                   state.error.c_str());
            }
        }

        /// @brief Draws the fields of a pose: the three coordinates and the facing.
        /// @return True on the frames a field changed.
        bool draw_pose(SushiDSP::Room::Pose& pose)
        {
            bool changed = false;
            ImGui::SetNextItemWidth(90.0f);
            changed = ImGui::InputDouble("x (m)", &pose.position.x, 0.05, 0.5, "%.2f") || changed;
            ImGui::SetNextItemWidth(90.0f);
            changed = ImGui::InputDouble("y (m)", &pose.position.y, 0.05, 0.5, "%.2f") || changed;
            ImGui::SetNextItemWidth(90.0f);
            changed =
                ImGui::InputDouble("height (m)", &pose.position.z, 0.05, 0.5, "%.2f") || changed;
            float yaw = static_cast<float>(pose.yaw_rad);
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::SliderAngle("facing", &yaw, -180.0f, 180.0f))
            {
                pose.yaw_rad = static_cast<double>(yaw);
                changed = true;
            }
            return changed;
        }

        /// @brief Draws the selected item's fields and writes its edits into the arrangement; a
        /// microphone's pattern and proximity are written to its own slot as well.
        /// @return True on the frames the arrangement changed.
        bool draw_inspector(RigController& rig, SushiDSP::Room::Arrangement& arrangement,
                            CabinetSectionState& state)
        {
            if (!state.scene.selected.has_value())
            {
                ImGui::TextColored(to_imvec4(palette().muted_text),
                                   "Select a marker in the room to place it by number.");
                return false;
            }
            const SceneItem selected = *state.scene.selected;
            if (selected.kind == SceneItemKind::Listener)
            {
                ImGui::TextUnformatted("Listener");
                SushiDSP::Room::Pose pose = arrangement.listener();
                if (draw_pose(pose))
                {
                    arrangement.set_listener(pose);
                    return true;
                }
                return false;
            }
            if (selected.kind == SceneItemKind::Cabinet)
            {
                const SushiDSP::Room::SourcePlacement* held = arrangement.find_source(selected.id);
                if (held == nullptr)
                {
                    state.scene.selected.reset();
                    return false;
                }
                ImGui::TextUnformatted("Cabinet");
                SushiDSP::Room::Pose pose = held->pose;
                bool changed = false;
                if (draw_pose(pose))
                {
                    arrangement.move_source(selected.id, pose);
                    changed = true;
                }
                SushiDSP::room::CabinetKind kind = SushiDSP::room::CabinetKind::FourByTwelve;
                const bool shipped = SushiDSP::room::cabinet_kind_of(held->cabinet, kind);
                int chosen = shipped ? static_cast<int>(kind) : -1;
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::Combo("cones", &chosen, K_CABINET_ITEMS) && chosen >= 0)
                {
                    arrangement.set_source_cabinet(
                        selected.id,
                        SushiDSP::room::cabinet_preset(
                                static_cast<SushiDSP::room::CabinetKind>(chosen))
                            .geometry);
                    changed = true;
                }
                return changed;
            }
            if (selected.kind == SceneItemKind::Furnishing)
            {
                SushiDSP::room::RoomScene shell = arrangement.shell();
                if (selected.id >= shell.furnishing_count || !shell.furnishings[selected.id].placed)
                {
                    state.scene.selected.reset();
                    return false;
                }
                SushiDSP::room::RoomScene::Furnishing& entry = shell.furnishings[selected.id];
                ImGui::TextUnformatted(SushiDSP::room::furniture(entry.item).name);
                bool changed = false;
                ImGui::SetNextItemWidth(90.0f);
                changed = ImGui::InputDouble("x (m)", &entry.x_m, 0.05, 0.5, "%.2f") || changed;
                ImGui::SetNextItemWidth(90.0f);
                changed = ImGui::InputDouble("y (m)", &entry.y_m, 0.05, 0.5, "%.2f") || changed;
                float yaw = static_cast<float>(entry.yaw_rad);
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::SliderAngle("turned", &yaw, -180.0f, 180.0f))
                {
                    entry.yaw_rad = static_cast<double>(yaw);
                    changed = true;
                }
                ImGui::TextColored(to_imvec4(palette().muted_text),
                                   "Absorbs around where it stands.\nIt casts no shadow.");
                if (changed)
                {
                    arrangement.set_shell(shell);
                }
                return changed;
            }

            const SushiDSP::Room::MicrophonePlacement* held =
                arrangement.find_microphone(selected.id);
            if (held == nullptr)
            {
                state.scene.selected.reset();
                return false;
            }
            ImGui::TextUnformatted("Microphone");
            SushiDSP::Room::MicrophonePlacement microphone = *held;
            bool changed = draw_pose(microphone.pose);
            if (const std::optional<MicrophoneAim> aim =
                    nearest_cone(arrangement, microphone.pose.position))
            {
                ImGui::TextColored(to_imvec4(palette().muted_text),
                                   "Cabinet %zu, cone %u: %.0f cm away,\n%.0f cm off its axis%s",
                                   aim->source + 1, aim->driver + 1u, aim->distance_m * 100.0,
                                   aim->off_axis_m * 100.0,
                                   aim->in_front_m < 0.0 ? ", behind the baffle" : "");
            }
            const SlotId slot = static_cast<SlotId>(selected.id);
            int pattern = static_cast<int>(microphone.pattern);
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::Combo("pattern", &pattern, K_PATTERN_ITEMS))
            {
                microphone.pattern = static_cast<SushiDSP::room::PolarPattern>(pattern);
                rig.set_parameter(slot, "pattern", static_cast<float>(pattern));
                changed = true;
            }
            if (ImGui::Checkbox("proximity effect", &microphone.proximity))
            {
                rig.set_parameter(slot, "proximity", microphone.proximity ? 1.0f : 0.0f);
                changed = true;
            }
            if (changed)
            {
                arrangement.update_microphone(microphone);
            }
            return changed;
        }
    } // namespace

    void DrawCabinetSectionPane(RigController& rig, RigTopology& topology,
                                TopologyApplier& applier, CabinetSectionState& state)
    {
        draw_toolbar(rig, topology, applier, state);
        const HiddenSlots slots = applier.mixer_slots();
        if (!slots.room.has_value())
        {
            ImGui::Spacing();
            ImGui::TextColored(to_imvec4(palette().muted_text),
                               "Place gear in the rig to open the cabinet section.");
            return;
        }

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float scene_width = std::max(available.x - K_INSPECTOR_WIDTH - 8.0f, 200.0f);
        const float scene_height =
            std::max(available.y - K_MIXER_HEIGHT - 8.0f, K_MIN_SCENE_HEIGHT);

        SushiDSP::Room::Arrangement arrangement = rig.room_arrangement(*slots.room);
        bool changed = false;

        ImGui::BeginGroup();
        changed = draw_room_scene("##room_scene", arrangement, state.scene,
                                  ImVec2(scene_width, scene_height)) ||
                  changed;
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::BeginChild("##inspector", ImVec2(0.0f, scene_height), true);
        changed = draw_inspector(rig, arrangement, state) || changed;
        if (changed)
        {
            std::string reason;
            if (rig.set_room_arrangement(*slots.room, arrangement, reason))
            {
                state.error.clear();
            }
            else
            {
                state.error = reason;
            }
        }
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Room"))
        {
            DrawRoomPanel(rig, *slots.room, state.room_error);
        }
        ImGui::EndChild();

        ImGui::BeginChild("##mixer", ImVec2(0.0f, 0.0f), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        const MixerEdit edit = draw_mixer_panel("##strips", rig, slots);
        ImGui::EndChild();
        if (edit.remove_microphone.has_value())
        {
            attempt(state.error, [&]
                    {
                        applier.remove_microphone(topology, rig, *edit.remove_microphone);
                    });
        }
        else if (edit.rewire)
        {
            attempt(state.error, [&]
                    {
                        applier.apply(topology, rig);
                    });
        }
    }
} // namespace ui
