/**************************************************************************/
/* StagePanel.cpp                                                         */
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

#include "ui/StagePanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "ui/FileBrowser.hpp"
#include "ui/PanelLibrary.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace ui
{
    namespace
    {
        /// @brief Finds a node type's registry entry.
        const SushiDSP::NodeDescriptor* descriptor_for(const std::string& id)
        {
            for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
            {
                if (id == entry.descriptor->id)
                {
                    return entry.descriptor;
                }
            }
            return nullptr;
        }

        /// @brief Finds one parameter by id within a node type.
        const SushiDSP::ParameterDescriptor* parameter_for(const SushiDSP::NodeDescriptor& node,
                                                           const char* param_id)
        {
            for (std::size_t p = 0; p < node.parameter_count; ++p)
            {
                if (std::strcmp(node.parameters[p].id, param_id) == 0)
                {
                    return &node.parameters[p];
                }
            }
            return nullptr;
        }

        /// @brief Fits a panel's aspect inside a region, centred.
        ImVec2 fit_panel(Vec2 aspect, ImVec2 region, ImVec2 origin, ImVec2& size_out)
        {
            const float scale = std::min(region.x / aspect.x, region.y / aspect.y);
            size_out = ImVec2(aspect.x * scale, aspect.y * scale);
            return ImVec2(origin.x + (region.x - size_out.x) * 0.5f,
                          origin.y + (region.y - size_out.y) * 0.5f);
        }

        /// @brief Installs one impulse response and reports what went wrong in the field's own
        /// error line.
        /// @param rig The rig that owns the slot.
        /// @param slot The node to install the cabinet on.
        /// @param path The impulse response's full path.
        /// @param error_out Receives the message, or "" when the cabinet went in.
        void load_cabinet(RigController& rig, RigController::SlotId slot, const std::string& path,
                          std::string& error_out)
        {
            error_out.clear();
            try
            {
                if (!rig.set_cabinet(slot, path))
                {
                    error_out = "Cabinet not installed: sample rate mismatch or unprepared node";
                }
            }
            catch (const std::exception& error)
            {
                error_out = error.what();
            }
        }

        /// @brief Draws the cabinet field below the panel of a node that hosts a cabinet.
        /// @param rig The rig that owns the slot.
        /// @param slot The node whose cabinet the field edits.
        /// @param default_ir_directory Where the browser opens when the rig remembers no folder.
        void draw_cabinet_field(RigController& rig, RigController::SlotId slot,
                                const std::string& default_ir_directory)
        {
            static char cabinet_input[512] = "";
            static std::optional<RigController::SlotId> field_slot;
            static std::string cabinet_error;
            static bool browser_open = false;
            static std::string browser_directory;

            const std::string cabinet = rig.cabinet_path(slot);
            if (field_slot != std::optional<RigController::SlotId>(slot))
            {
                field_slot = slot;
                std::snprintf(cabinet_input, sizeof(cabinet_input), "%s", cabinet.c_str());
                cabinet_error.clear();
                browser_open = false;
            }

            ImGui::Separator();
            ImGui::Text("Cabinet: %s", cabinet.empty() ? "(none)" : cabinet.c_str());
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("##cabinet_ir", cabinet_input, sizeof(cabinet_input));
            ImGui::SameLine();
            if (ImGui::Button("Load##cabinet"))
            {
                load_cabinet(rig, slot, cabinet_input, cabinet_error);
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse##cabinet"))
            {
                browser_open = !browser_open;
                if (browser_open)
                {
                    const std::string remembered = rig.ir_folder();
                    browser_directory = remembered.empty() ? default_ir_directory : remembered;
                }
            }

            if (browser_open)
            {
                const std::string opened_at = browser_directory;
                const std::optional<std::string> picked =
                    FileBrowser("cabinet_browser", browser_directory, ".wav");
                if (browser_directory != opened_at)
                {
                    rig.set_ir_folder(browser_directory);
                }
                if (picked.has_value())
                {
                    std::snprintf(cabinet_input, sizeof(cabinet_input), "%s", picked->c_str());
                    load_cabinet(rig, slot, *picked, cabinet_error);
                    browser_open = false;
                }
            }

            if (!cabinet_error.empty())
            {
                ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", cabinet_error.c_str());
            }
        }
    }

    void DrawStagePanel(RigController& rig, std::optional<RigController::SlotId> selected,
                        const std::string& default_ir_directory)
    {
        if (!selected.has_value())
        {
            ImGui::TextColored(to_imvec4(palette().muted_text),
                "Drag gear from the browser onto the chain, then click it to edit.");
            return;
        }

        const RigController::SlotId slot = *selected;
        const std::string id = rig.descriptor_id(slot);
        const SushiDSP::NodeDescriptor* node = descriptor_for(id);
        if (node == nullptr)
        {
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "Unknown node type '%s'", id.c_str());
            return;
        }

        std::vector<ControlPlacement> generated;
        const PanelDescriptor* table_entry = panel_for(id.c_str());
        const PanelDescriptor panel = table_entry != nullptr ? *table_entry
                                                             : auto_panel(*node, generated);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 region = ImGui::GetContentRegionAvail();
        const ImVec2 panel_region(region.x, std::max(80.0f, region.y - 90.0f));
        ImVec2 panel_size;
        const ImVec2 panel_origin = fit_panel(panel.aspect, panel_region, origin, panel_size);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(panel_origin,
                            ImVec2(panel_origin.x + panel_size.x, panel_origin.y + panel_size.y),
                            to_imu32(panel.skin.body), 8.0f);
        draw->AddRectFilled(ImVec2(panel_origin.x + panel_size.x * 0.04f,
                                   panel_origin.y + panel_size.y * 0.58f),
                            ImVec2(panel_origin.x + panel_size.x * 0.96f,
                                   panel_origin.y + panel_size.y * 0.95f),
                            to_imu32(panel.skin.face), 4.0f);
        draw->AddText(ImVec2(panel_origin.x + panel_size.x * 0.06f,
                             panel_origin.y + panel_size.y * 0.10f),
                      to_imu32(panel.skin.accent), node->name);

        const std::size_t upstream_count = rig.upstream_of(slot).size();
        for (std::size_t c = 0; c < panel.control_count; ++c)
        {
            const ControlPlacement& placement = panel.controls[c];
            if (panel.is_control_visible != nullptr && !panel.is_control_visible(c, upstream_count))
            {
                continue;
            }
            const SushiDSP::ParameterDescriptor* param = parameter_for(*node, placement.param_id);
            if (param == nullptr)
            {
                continue;
            }

            const float diameter = placement.size * panel_size.y;
            const ImVec2 center(panel_origin.x + placement.position.x * panel_size.x,
                                panel_origin.y + placement.position.y * panel_size.y);
            ImGui::SetCursorScreenPos(ImVec2(center.x - diameter * 0.5f, center.y - diameter * 0.5f));

            float value = rig.get_parameter(slot, param->id);
            bool changed = false;
            if (placement.kind == ControlKind::Switch)
            {
                bool on = value >= 0.5f;
                changed = ToggleSwitch(param->id, &on, diameter, diameter * 0.5f);
                value = on ? 1.0f : 0.0f;
            }
            else if (placement.kind == ControlKind::Readout)
            {
                ValueReadout(param->id, *param, value, diameter, diameter * 0.5f);
            }
            else if (placement.kind == ControlKind::Slider)
            {
                const float width = diameter * kSliderWidthRatio;
                ImGui::SetCursorScreenPos(ImVec2(center.x - width * 0.5f,
                                                 center.y - diameter * 0.5f));
                changed = Slider(param->id, *param, &value, diameter);
            }
            else
            {
                changed = Knob(param->id, *param, &value, diameter);
            }
            if (changed)
            {
                rig.set_parameter(slot, param->id, value);
            }

            const char* caption = placement.caption != nullptr ? placement.caption : param->label;
            const ImVec2 text_size = ImGui::CalcTextSize(caption);
            draw->AddText(ImVec2(center.x - text_size.x * 0.5f, center.y + diameter * 0.62f),
                          to_imu32(panel.skin.text), caption);
        }

        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + panel_region.y + 8.0f));
        if (rig.hosts_cabinet(slot))
        {
            draw_cabinet_field(rig, slot, default_ir_directory);
        }
        if (rig.dropped_parameter_changes() > 0)
        {
            ImGui::TextColored(to_imvec4(rgba(255, 178, 76)), "Dropped parameter changes: %zu",
                               rig.dropped_parameter_changes());
        }
    }
}
