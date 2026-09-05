/**************************************************************************/
/* ChainStrip.cpp                                                         */
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

#include "ui/ChainStrip.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "SushiDSP/routing/SplitterNode.hpp"

#include "SlotSelection.hpp"
#include "ui/ChainLayout.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace ui
{
    namespace
    {
        constexpr float kCardWidth = 132.0f;
        constexpr float kCardHeight = 78.0f;
        constexpr float kColumnGap = 46.0f;
        constexpr float kRowGap = 22.0f;

        /// @brief Collects the rig's edges as the pairs layout_chain takes.
        std::vector<std::pair<RigController::SlotId, RigController::SlotId>> collect_edges(
            const RigController& rig, const std::vector<RigController::SlotId>& slots)
        {
            std::vector<std::pair<RigController::SlotId, RigController::SlotId>> edges;
            for (RigController::SlotId slot : slots)
            {
                for (RigController::SlotId upstream : rig.upstream_of(slot))
                {
                    edges.push_back({ upstream, slot });
                }
            }
            return edges;
        }

        /// @brief Collects the slots a user may start a parallel branch from.
        std::vector<RigController::SlotId> collect_branchable(
            const RigController& rig, const std::vector<RigController::SlotId>& slots)
        {
            std::vector<RigController::SlotId> branchable;
            for (RigController::SlotId slot : slots)
            {
                if (rig.descriptor_id(slot) == SushiDSP::splitter_descriptor().id)
                {
                    branchable.push_back(slot);
                }
            }
            return branchable;
        }

        /// @brief Runs one rig edit and keeps the failure inside the strip. Every structural edit
        /// republishes, and a republish throws when the resulting graph will not compile — a
        /// throw let out of here would unwind past ImGui's unmatched End() and terminate the
        /// process. RigController::publish() builds and compiles before it swaps, so a rejected
        /// edit leaves the graph the audio thread is playing exactly as it was.
        /// @param edit The edit to attempt.
        /// @param error Out: the failure message, cleared by an edit that succeeds.
        template <typename Edit>
        void run_edit(Edit&& edit, std::string& error)
        {
            try
            {
                edit();
                error.clear();
            }
            catch (const std::exception& failure)
            {
                error = failure.what();
            }
        }

        /// @brief Outlines a rectangle in dashes, for a card-sized slot that holds no card yet.
        /// @param draw The window's draw list.
        /// @param top_left The rectangle's upper-left corner.
        /// @param bottom_right The rectangle's lower-right corner.
        /// @param color The dash color.
        void draw_dashed_rect(ImDrawList* draw, ImVec2 top_left, ImVec2 bottom_right, ImU32 color)
        {
            constexpr float kDash = 8.0f;
            constexpr float kGap = 6.0f;
            const ImVec2 corners[4] = { top_left, ImVec2(bottom_right.x, top_left.y),
                                        bottom_right, ImVec2(top_left.x, bottom_right.y) };
            for (int side = 0; side < 4; ++side)
            {
                const ImVec2 from = corners[side];
                const ImVec2 to = corners[(side + 1) % 4];
                const float length = std::abs(to.x - from.x) + std::abs(to.y - from.y);
                if (length <= 0.0f)
                {
                    continue;
                }
                const float step_x = (to.x - from.x) / length;
                const float step_y = (to.y - from.y) / length;
                for (float at = 0.0f; at < length; at += kDash + kGap)
                {
                    const float dash_end = std::min(at + kDash, length);
                    draw->AddLine(ImVec2(from.x + step_x * at, from.y + step_y * at),
                                  ImVec2(from.x + step_x * dash_end, from.y + step_y * dash_end),
                                  color, 1.5f);
                }
            }
        }

        /// @brief Draws an arrow from one card's right edge to another card's left edge.
        void draw_link(ImDrawList* draw, ImVec2 from, ImVec2 to, ImU32 color)
        {
            if (from.y == to.y)
            {
                draw->AddLine(from, to, color, 2.0f);
            }
            else
            {
                const float mid_x = (from.x + to.x) * 0.5f;
                draw->AddBezierCubic(from, ImVec2(mid_x, from.y), ImVec2(mid_x, to.y), to,
                                     color, 2.0f);
            }
            draw->AddTriangleFilled(ImVec2(to.x, to.y),
                                    ImVec2(to.x - 8.0f, to.y - 5.0f),
                                    ImVec2(to.x - 8.0f, to.y + 5.0f), color);
        }
    }

    void DrawChainStrip(RigController& rig, std::optional<RigController::SlotId>& selected,
                        std::optional<RigController::SlotId>& created_slot,
                        std::string& edit_error)
    {
        created_slot = std::nullopt;

        const std::vector<RigController::SlotId> slots = rig.slot_order();
        const ChainView view =
            layout_chain(slots, collect_edges(rig, slots), collect_branchable(rig, slots));

        const Palette& colors = palette();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Links are drawn first so cards sit on top of them.
        for (const ChainLink& link : view.links)
        {
            const ChainCell& from = view.cells[link.from_cell];
            const ChainCell& to = view.cells[link.to_cell];
            const ImVec2 from_point(
                origin.x + static_cast<float>(from.column) * (kCardWidth + kColumnGap) + kCardWidth,
                origin.y + static_cast<float>(from.row) * (kCardHeight + kRowGap) + kCardHeight * 0.5f);
            const ImVec2 to_point(
                origin.x + static_cast<float>(to.column) * (kCardWidth + kColumnGap),
                origin.y + static_cast<float>(to.row) * (kCardHeight + kRowGap) + kCardHeight * 0.5f);
            draw_link(draw, from_point, to_point, to_imu32(colors.muted_text));
        }

        for (const ChainCell& cell : view.cells)
        {
            const ImVec2 card_origin(
                origin.x + static_cast<float>(cell.column) * (kCardWidth + kColumnGap),
                origin.y + static_cast<float>(cell.row) * (kCardHeight + kRowGap));
            const ImVec2 card_end(card_origin.x + kCardWidth, card_origin.y + kCardHeight);
            const bool bypassed = rig.is_bypassed(cell.slot);

            ImGui::PushID(static_cast<int>(cell.slot));
            ImGui::SetCursorScreenPos(card_origin);
            // The bypass switch and the remove button sit inside this rectangle; without the
            // opt-in the card would hold the hover id and neither of them could ever be clicked.
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("card", ImVec2(kCardWidth, kCardHeight));
            if (ImGui::IsItemClicked())
            {
                selected = cell.slot;
            }

            const bool is_selected = selected == std::optional<RigController::SlotId>(cell.slot);
            draw->AddRectFilled(card_origin, card_end,
                                to_imu32(rgba(38, 38, 44, bypassed ? 115 : 255)), 6.0f);
            draw->AddRect(card_origin, card_end,
                          to_imu32(is_selected ? colors.accent : rgba(62, 62, 70)), 6.0f, 0,
                          is_selected ? 2.5f : 1.0f);
            const std::string label = rig.descriptor_id(cell.slot);
            draw->AddText(ImVec2(card_origin.x + 10.0f, card_origin.y + 10.0f),
                          to_imu32(bypassed ? colors.muted_text : colors.text), label.c_str());

            if (ImGui::BeginDragDropSource())
            {
                const RigController::SlotId payload = cell.slot;
                ImGui::SetDragDropPayload("RACK_SLOT_ID", &payload, sizeof(RigController::SlotId));
                ImGui::Text("%s", label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RACK_SLOT_ID"))
                {
                    const RigController::SlotId moved =
                        *static_cast<const RigController::SlotId*>(payload->Data);
                    run_edit([&] { rig.move_before(moved, cell.slot); }, edit_error);
                }
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    run_edit([&] { created_slot = rig.insert_before(cell.slot, std::string(descriptor_id)); },
                             edit_error);
                    if (created_slot.has_value())
                    {
                        selected = created_slot;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SetCursorScreenPos(ImVec2(card_origin.x + 10.0f, card_end.y - 26.0f));
            bool active = !bypassed;
            if (ToggleSwitch("bypass", &active, 34.0f, 16.0f))
            {
                run_edit([&] { rig.set_bypassed(cell.slot, !active); }, edit_error);
            }

            ImGui::SetCursorScreenPos(ImVec2(card_end.x - 26.0f, card_end.y - 26.0f));
            if (ImGui::SmallButton("x"))
            {
                run_edit([&] { rig.remove(cell.slot); }, edit_error);
            }
            ImGui::PopID();
        }

        for (const OpenBranch& offer : view.open_branches)
        {
            const ImVec2 offer_origin(
                origin.x + static_cast<float>(offer.column) * (kCardWidth + kColumnGap),
                origin.y + static_cast<float>(offer.row) * (kCardHeight + kRowGap));
            const ImVec2 offer_end(offer_origin.x + kCardWidth, offer_origin.y + kCardHeight);

            ImGui::PushID(static_cast<int>(offer.source));
            ImGui::SetCursorScreenPos(offer_origin);
            ImGui::InvisibleButton("open_branch", ImVec2(kCardWidth, kCardHeight));
            draw_dashed_rect(draw, offer_origin, offer_end, to_imu32(colors.muted_text));
            draw->AddText(ImVec2(offer_origin.x + 10.0f, offer_origin.y + 10.0f),
                          to_imu32(colors.muted_text), "drop to branch");
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
                {
                    const char* descriptor_id = static_cast<const char*>(payload->Data);
                    run_edit([&] { created_slot = rig.add_branch(offer.source, std::string(descriptor_id)); },
                             edit_error);
                    if (created_slot.has_value())
                    {
                        selected = created_slot;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }

        // The drop zone past the last column appends.
        const ImVec2 tail(origin.x + static_cast<float>(view.column_count) * (kCardWidth + kColumnGap),
                          origin.y);
        ImGui::SetCursorScreenPos(tail);
        ImGui::InvisibleButton("append_zone", ImVec2(kCardWidth, kCardHeight));
        draw->AddRect(tail, ImVec2(tail.x + kCardWidth, tail.y + kCardHeight),
                      to_imu32(rgba(62, 62, 70)), 6.0f, 0, 1.0f);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID"))
            {
                const char* descriptor_id = static_cast<const char*>(payload->Data);
                run_edit([&] { created_slot = rig.append(std::string(descriptor_id)); }, edit_error);
                if (created_slot.has_value())
                {
                    selected = created_slot;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (!slot_selection::still_valid(selected, rig.slot_order()))
        {
            selected = std::nullopt;
        }
    }
}
