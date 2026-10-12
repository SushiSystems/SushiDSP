/**************************************************************************/
/* cabinet_section_window.cpp                                             */
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
 * @file cabinet_section_window.cpp
 * @brief Implements the cabinet section window.
 * @author Mustafa Garip
 */

#include "ui/cabinet_section_window.hpp"

#include <cstddef>
#include <exception>
#include <string>

#include <imgui.h>

#include "ui/cabinet_section_view.hpp"
#include "ui/mixer_panel.hpp"

namespace ui
{
    namespace
    {
        /// @brief Places the cabinet of one path at the room's default spot for that path: centred
        /// across the room, spaced along its depth, facing the listener's side.
        /// @return The placement.
        RoomPlacement default_cabinet_place(const RoomLayout& layout, std::size_t path)
        {
            const SushiDSP::room::Vec3 dimensions = layout.shell().dimensions;
            RoomPlacement place;
            place.position = SushiDSP::room::Vec3{0.5 * dimensions.x,
                                                  (0.25 + 0.25 * static_cast<double>(path)) *
                                                      dimensions.y,
                                                  0.8};
            place.yaw_rad = 0.0;
            return place;
        }

        /// @brief Draws the room and DI switches and applies each change through the applier.
        void draw_switches(RigController& rig, RigTopology& topology, TopologyApplier& applier,
                           std::string& error)
        {
            bool room_open = topology.room();
            if (ImGui::Checkbox("Room open", &room_open))
            {
                try
                {
                    applier.set_room(topology, rig, room_open);
                    error.clear();
                }
                catch (const std::exception& refusal)
                {
                    error = refusal.what();
                }
            }

            bool di_open = topology.di();
            if (ImGui::Checkbox("DI path", &di_open))
            {
                try
                {
                    applier.set_di(topology, rig, di_open);
                    error.clear();
                }
                catch (const std::exception& refusal)
                {
                    error = refusal.what();
                }
            }
        }

        /// @brief Offers a button for each path whose cabinet is not placed yet.
        /// @return True when a cabinet was placed this frame.
        bool draw_unplaced_cabinets(RoomLayout& layout, std::size_t paths)
        {
            bool placed = false;
            for (std::size_t path = layout.cabinets().size(); path < paths; ++path)
            {
                const std::string label = "Place cabinet " + std::to_string(path);
                if (ImGui::Button(label.c_str()))
                {
                    std::string reason;
                    placed = layout.set_cabinet(path, default_cabinet_place(layout, path), reason) ||
                             placed;
                    if (!placed)
                    {
                        ImGui::TextUnformatted(reason.c_str());
                    }
                    break;
                }
            }
            return placed;
        }
    } // namespace

    void DrawCabinetSectionWindow(RigController& rig, RigTopology& topology,
                                  TopologyApplier& applier, std::string& error)
    {
        ImGui::SetNextWindowSize(ImVec2(440.0f, 640.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Cabinet section"))
        {
            ImGui::End();
            return;
        }

        draw_switches(rig, topology, applier, error);

        if (topology.room())
        {
            RoomLayout& layout = rig.room_layout();
            const std::size_t paths = RigTopology::branch_count(topology.chain_type());
            const bool placed = draw_unplaced_cabinets(layout, paths);
            const bool edited = draw_cabinet_section_view("##plan", layout, 260.0f);
            if (placed || edited)
            {
                try
                {
                    applier.apply(topology, rig);
                    error.clear();
                }
                catch (const std::exception& refusal)
                {
                    error = refusal.what();
                }
            }
        }

        if (!error.empty())
        {
            ImGui::TextWrapped("%s", error.c_str());
        }

        ImGui::Separator();
        draw_mixer_panel("##mixer", rig, applier.mixer_slots());
        ImGui::End();
    }
} // namespace ui
