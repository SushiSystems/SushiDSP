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
#include <cstdio>
#include <cstdint>
#include <optional>

#include <imgui.h>

#include "SushiDSP/room/room_arrangement.hpp"

#include "ui/mixer_panel.hpp"
#include "SushiDSP/room/cabinet_geometry.hpp"
#include "SushiDSP/room/furniture.hpp"
#include "SushiDSP/room/microphone_model.hpp"
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
            changed = ImGui::InputDouble("x (m)", &pose.position.x, 0.01, 0.1, "%.3f") || changed;
            ImGui::SetNextItemWidth(90.0f);
            changed = ImGui::InputDouble("y (m)", &pose.position.y, 0.01, 0.1, "%.3f") || changed;
            ImGui::SetNextItemWidth(90.0f);
            changed =
                ImGui::InputDouble("height (m)", &pose.position.z, 0.01, 0.1, "%.3f") || changed;
            float yaw = static_cast<float>(pose.yaw_rad);
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::SliderAngle("facing", &yaw, -180.0f, 180.0f))
            {
                pose.yaw_rad = static_cast<double>(yaw);
                changed = true;
            }
            return changed;
        }

        /// @brief Draws the list of shipped microphones, each named with how it transduces, and
        /// the chosen one's source under it.
        /// @return True on the frames another microphone was chosen.
        bool draw_microphone_kind(SushiDSP::room::MicrophoneKind& kind)
        {
            const auto label_of = [](const SushiDSP::room::MicrophonePreset& preset, char* into,
                                     std::size_t size)
            {
                std::snprintf(into, size, "%s (%s)", preset.name, preset.transducer);
            };
            const SushiDSP::room::MicrophonePreset& current =
                SushiDSP::room::microphone_preset(kind);
            char label[64];
            label_of(current, label, sizeof(label));
            bool changed = false;
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo("microphone", label))
            {
                for (std::size_t index = 0; index < SushiDSP::room::K_MICROPHONE_KIND_COUNT;
                     ++index)
                {
                    const auto candidate = static_cast<SushiDSP::room::MicrophoneKind>(index);
                    char row[64];
                    label_of(SushiDSP::room::microphone_preset(candidate), row, sizeof(row));
                    if (ImGui::Selectable(row, candidate == kind) && candidate != kind)
                    {
                        kind = candidate;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextColored(to_imvec4(palette().muted_text), "On-axis curve: %s.",
                               current.citation);
            return changed;
        }

        /// @brief Draws a microphone's place against a cone: which cone, how far in front of the
        /// baffle, and how far aside of and above the cone's axis, each to the centimetre.
        /// @return True on the frames the pose changed.
        bool draw_cone_placement(const SushiDSP::Room::Arrangement& arrangement,
                                 SushiDSP::Room::Pose& pose)
        {
            const std::optional<MicrophoneAim> aim = nearest_cone(arrangement, pose.position);
            if (!aim.has_value())
            {
                return false;
            }
            const auto& sources = arrangement.sources();
            std::size_t source = aim->source;
            std::uint32_t driver = aim->driver;
            double in_front_cm = aim->in_front_m * 100.0;
            double aside_cm = aim->aside_m * 100.0;
            double above_cm = aim->above_m * 100.0;
            bool changed = false;

            ImGui::Separator();
            char current[48];
            std::snprintf(current, sizeof(current), "Cabinet %zu, %s", source + 1,
                          cone_label(sources[source].cabinet, driver));
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::BeginCombo("cone", current))
            {
                for (std::size_t cabinet = 0; cabinet < sources.size(); ++cabinet)
                {
                    const SushiDSP::room::CabinetGeometry& geometry = sources[cabinet].cabinet;
                    for (std::uint32_t cone = 0; cone < geometry.driver_count(); ++cone)
                    {
                        char label[48];
                        std::snprintf(label, sizeof(label), "Cabinet %zu, %s", cabinet + 1,
                                      cone_label(geometry, cone));
                        if (ImGui::Selectable(label, cabinet == source && cone == driver))
                        {
                            // A newly chosen cone is taken on its axis, at the distance held.
                            source = cabinet;
                            driver = cone;
                            in_front_cm = std::max(in_front_cm, 1.0);
                            aside_cm = 0.0;
                            above_cm = 0.0;
                            changed = true;
                        }
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("in front (cm)", &in_front_cm, 1.0, 5.0, "%.1f") || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("aside (cm)", &aside_cm, 0.5, 2.0, "%.1f") || changed;
            ImGui::SetNextItemWidth(110.0f);
            changed = ImGui::InputDouble("above (cm)", &above_cm, 0.5, 2.0, "%.1f") || changed;
            ImGui::TextColored(to_imvec4(palette().muted_text),
                               "0 aside and 0 above is the dust cap;\n%.1f cm aside is the cone's "
                               "edge.",
                               sources[source].cabinet.cone_radius_m * 100.0);
            if (changed)
            {
                pose = microphone_on_cone(sources[source].pose, sources[source].cabinet, driver,
                                          in_front_cm * 0.01, aside_cm * 0.01, above_cm * 0.01);
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
            SushiDSP::Room::MicrophonePlacement microphone = *held;
            bool changed = draw_microphone_kind(microphone.kind);
            changed = draw_pose(microphone.pose) || changed;
            changed = draw_cone_placement(arrangement, microphone.pose) || changed;
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

        const JoinContent join = applier.join_content(topology, rig);
        if (join != JoinContent::Cabinets)
        {
            ImGui::TextColored(to_imvec4(rgba(214, 168, 74)), "%s",
                               join == JoinContent::Line
                                   ? "No cabinet in the rig. An amplifier with no speaker moves "
                                     "no air: the room and its microphones are silent, and the "
                                     "chain is heard as a line signal on the Line strip."
                                   : "A path has no cabinet. It moves no air, so the room does "
                                     "not hear it; it is heard as a line signal, summed with the "
                                     "cabinets on the Cabinet + Line strip.");
        }

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
        const MixerEdit edit = draw_mixer_panel("##strips", rig, slots, join);
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
