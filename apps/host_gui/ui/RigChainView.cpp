/****************************************************************/
/* RigChainView.cpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/** @file RigChainView.cpp
 *  @brief Draws the chain-type rail and the routed rig chain, and applies their edits.
 *  @author Mustafa Garip
 */

#include "ui/RigChainView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

#include "SlotSelection.hpp"
#include "ui/SplitLayout.hpp"
#include "ui/Theme.hpp"
#include "ui/Widgets.hpp"

namespace ui
{
    namespace
    {
        constexpr Color kTeal = rgba(95, 179, 163);
        constexpr Color kTealSoft = rgba(95, 179, 163, 40);
        constexpr Color kCardFill = rgba(38, 38, 44);
        constexpr Color kCardFillBypassed = rgba(38, 38, 44, 115);
        constexpr Color kCardBorder = rgba(62, 62, 70);
        constexpr Color kIoFill = rgba(48, 48, 56);
        constexpr float kButtonSize = 38.0f;
        constexpr float kIconSize = 15.0f;
        constexpr float kIconThickness = 1.5f;
        constexpr float kRailPadding = 8.0f;
        constexpr float kCanvasPadding = 16.0f;
        constexpr float kCableThickness = 3.0f;
        constexpr float kWireThickness = 2.0f;
        constexpr float kCaretWidth = 3.0f;
        constexpr float kCaretHeight = 34.0f;
        constexpr float kBadgeRadius = 8.0f;

        constexpr std::array<ChainType, 3> kChainTypes = { ChainType::Single, ChainType::TwoWay,
                                                          ChainType::ThreeWay };

        /// @brief Pairs a stage's address with the geometry layout_split gave it.
        struct StageView
        {
            StageAddress         address;  ///< Names the stage in the topology.
            const StageGeometry* geometry; ///< Points into the frame's SplitGeometry.
        };

        /// @brief Runs one edit, restoring the topology and recording the failure when it throws.
        /// @param state The view state whose topology is restored and whose edit_error is written.
        /// @param edit The edit to attempt.
        void run_edit(RigChainState& state, const std::function<void()>& edit)
        {
            const RigTopology before = state.topology;
            try
            {
                edit();
                state.edit_error.clear();
            }
            catch (const std::exception& failure)
            {
                state.topology = before;
                state.edit_error = failure.what();
            }
        }

        /// @brief Maps a panel-local point to screen space.
        ImVec2 to_screen(ImVec2 origin, Vec2 point)
        {
            return ImVec2(origin.x + point.x, origin.y + point.y);
        }

        /// @brief Strokes one of the mockup's chain-type icons, its 24-unit viewBox scaled to `size`.
        void draw_chain_icon(ImDrawList* draw, ChainType type, ImVec2 center, float size, ImU32 color)
        {
            const float scale = size / 24.0f;
            const auto at = [&](float x, float y) {
                return ImVec2(center.x + (x - 12.0f) * scale, center.y + (y - 12.0f) * scale);
            };
            const auto branch = [&](float fork_x, float end_y) {
                draw->PathLineTo(at(fork_x, 12.0f));
                draw->PathBezierCubicCurveTo(at(fork_x + 2.0f, 12.0f), at(fork_x + 2.0f, end_y),
                                             at(fork_x + 5.0f, end_y));
                draw->PathLineTo(at(21.0f, end_y));
                draw->PathStroke(color, ImDrawFlags_None, kIconThickness);
            };
            switch (type)
            {
            case ChainType::Single:
                draw->AddLine(at(3.0f, 12.0f), at(21.0f, 12.0f), color, kIconThickness);
                break;
            case ChainType::TwoWay:
                draw->AddLine(at(3.0f, 12.0f), at(8.0f, 12.0f), color, kIconThickness);
                branch(8.0f, 7.0f);
                branch(8.0f, 17.0f);
                break;
            case ChainType::ThreeWay:
                draw->AddLine(at(3.0f, 12.0f), at(21.0f, 12.0f), color, kIconThickness);
                branch(7.0f, 5.0f);
                branch(7.0f, 19.0f);
                break;
            }
        }

        /// @brief Looks up a descriptor's display name in the node registry.
        /// @return The name, or the id itself when the registry does not hold it.
        std::string display_name(const std::string& descriptor_id)
        {
            for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
            {
                if (descriptor_id == entry.descriptor->id)
                {
                    return entry.descriptor->name;
                }
            }
            return descriptor_id;
        }

        /// @brief Outlines a rectangle in dashes, for an empty branch's drop placeholder.
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

        /// @brief Draws the chain-type rail and switches the chain type on a click.
        void draw_rail(RigController& rig, RigChainState& state)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            for (ChainType type : kChainTypes)
            {
                ImGui::PushID(static_cast<int>(type));
                const ImVec2 top_left = ImGui::GetCursorScreenPos();
                const bool active = state.topology.chain_type() == type;
                if (ImGui::InvisibleButton("chain_type", ImVec2(kButtonSize, kButtonSize)) && !active)
                {
                    run_edit(state, [&] {
                        state.applier.switch_chain_type(state.topology, rig, type);
                    });
                    state.selected = std::nullopt;
                }
                const bool hovered = ImGui::IsItemHovered();
                const ImVec2 bottom_right(top_left.x + kButtonSize, top_left.y + kButtonSize);
                draw->AddRectFilled(top_left, bottom_right, to_imu32(hovered ? kCardBorder : kCardFill), 6.0f);
                draw->AddRect(top_left, bottom_right, to_imu32(active ? kTeal : kCardBorder), 6.0f, 0,
                              active ? 1.5f : 1.0f);
                const ImVec2 center(top_left.x + kButtonSize / 2.0f, top_left.y + kButtonSize / 2.0f);
                draw_chain_icon(draw, type, center, kIconSize,
                                to_imu32(active ? kTeal : palette().muted_text));
                ImGui::PopID();
            }
        }

        /// @brief Collects every stage the active chain type shows, with its geometry.
        std::vector<StageView> collect_stages(ChainType type, const SplitGeometry& geometry)
        {
            std::vector<StageView> stages;
            // One path is the run itself and carries no master bus, so it is the only stage the
            // view shows; more than one puts each path in its own row, then the master bus.
            if (RigTopology::branch_count(type) == 1)
            {
                stages.push_back({ StageAddress{ StageKind::Branch, 0 }, &geometry.series });
                return stages;
            }
            for (std::size_t i = 0; i < geometry.branches.size(); ++i)
            {
                stages.push_back({ StageAddress{ StageKind::Branch, i }, &geometry.branches[i] });
            }
            stages.push_back({ StageAddress{ StageKind::Post, 0 }, &geometry.post });
            return stages;
        }

        /// @brief Draws the wires across a stage's gaps, or an empty branch's dashed placeholder.
        void draw_stage_wires(ImDrawList* draw, ImVec2 origin, const StageView& stage)
        {
            const StageGeometry& geometry = *stage.geometry;
            const float center_y = origin.y + (geometry.bounds.min.y + geometry.bounds.max.y) / 2.0f;
            if (stage.address.kind == StageKind::Branch && geometry.nodes.empty())
            {
                const Rect& gap = geometry.gaps.front();
                draw_dashed_rect(draw, to_screen(origin, gap.min), to_screen(origin, gap.max),
                                 to_imu32(palette().muted_text));
            }
            else
            {
                for (const Rect& gap : geometry.gaps)
                {
                    draw->AddLine(ImVec2(origin.x + gap.min.x, center_y), ImVec2(origin.x + gap.max.x, center_y),
                                  to_imu32(palette().accent), kWireThickness);
                }
            }
            // A branch shorter than the widest one is padded out to the mixer's cable by this run.
            const float tail_x = geometry.gaps.back().max.x;
            if (tail_x < geometry.bounds.max.x)
            {
                draw->AddLine(ImVec2(origin.x + tail_x, center_y),
                              ImVec2(origin.x + geometry.bounds.max.x, center_y), to_imu32(kTeal),
                              kCableThickness);
            }
        }

        /// @brief Draws one node card and queues the edit its bypass toggle or remove button makes.
        void draw_node(RigController& rig, RigChainState& state, SlotId slot, ImVec2 top_left,
                       ImVec2 bottom_right, std::function<void()>& pending)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const Palette& colors = palette();
            const bool bypassed = rig.is_bypassed(slot);
            const std::string name = display_name(rig.descriptor_id(slot));
            const ImU32 text_color = to_imu32(bypassed ? colors.muted_text : colors.text);

            ImGui::PushID(static_cast<int>(slot));
            ImGui::SetCursorScreenPos(top_left);
            // The bypass switch and the remove button sit inside the card and must win its hover.
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("card", ImVec2(bottom_right.x - top_left.x, bottom_right.y - top_left.y));
            if (ImGui::IsItemClicked())
            {
                state.selected = slot;
            }
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("RACK_SLOT_ID", &slot, sizeof(SlotId));
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }

            const bool is_selected = state.selected == std::optional<SlotId>(slot);
            draw->AddRectFilled(top_left, bottom_right, to_imu32(bypassed ? kCardFillBypassed : kCardFill), 9.0f);
            draw->AddRect(top_left, bottom_right, to_imu32(is_selected ? colors.accent : kCardBorder), 9.0f, 0,
                          is_selected ? 2.5f : 1.0f);

            const float center_x = (top_left.x + bottom_right.x) / 2.0f;
            const ImVec2 icon_min(center_x - 11.0f, top_left.y + 10.0f);
            const ImVec2 icon_max(center_x + 11.0f, top_left.y + 32.0f);
            draw->AddRectFilled(icon_min, icon_max, to_imu32(kCardBorder), 5.0f);
            const char initial[2] = { name.empty() ? '?' : name.front(), '\0' };
            const ImVec2 initial_size = ImGui::CalcTextSize(initial);
            draw->AddText(ImVec2(center_x - initial_size.x / 2.0f, (icon_min.y + icon_max.y - initial_size.y) / 2.0f),
                          text_color, initial);

            const ImVec4 clip(top_left.x + 4.0f, top_left.y, bottom_right.x - 4.0f, bottom_right.y);
            const float name_x = std::max(top_left.x + 4.0f, center_x - ImGui::CalcTextSize(name.c_str()).x / 2.0f);
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(name_x, top_left.y + 38.0f), text_color,
                          name.c_str(), nullptr, 0.0f, &clip);

            ImGui::SetCursorScreenPos(ImVec2(top_left.x + 6.0f, bottom_right.y - 22.0f));
            bool active = !bypassed;
            if (ToggleSwitch("bypass", &active, 30.0f, 14.0f))
            {
                const bool now_bypassed = !active;
                pending = [&rig, slot, now_bypassed] { rig.set_bypassed(slot, now_bypassed); };
            }

            ImGui::SetCursorScreenPos(ImVec2(bottom_right.x - 20.0f, bottom_right.y - 24.0f));
            if (ImGui::SmallButton("x"))
            {
                // The applier removes the slot from the rig once the topology stops routing it.
                pending = [&rig, &state, slot] {
                    state.topology.remove(slot);
                    state.applier.apply(state.topology, rig);
                };
            }
            ImGui::PopID();
        }

        /// @brief Queues the edit a delivered gear or slot payload makes at one stage gap.
        void queue_drop(RigController& rig, RigChainState& state, std::optional<SlotId>& created,
                        const ImGuiPayload& payload, bool is_gear, StageAddress address, std::size_t index,
                        std::function<void()>& pending)
        {
            if (is_gear)
            {
                const std::string descriptor_id(static_cast<const char*>(payload.Data));
                pending = [&rig, &state, &created, descriptor_id, address, index] {
                    const SlotId slot = rig.add_node(descriptor_id, false);
                    try
                    {
                        state.topology.insert(address, index, slot);
                        state.applier.apply(state.topology, rig);
                    }
                    catch (...)
                    {
                        rig.remove(slot, false);
                        throw;
                    }
                    state.selected = slot;
                    created = slot;
                };
                return;
            }
            const SlotId slot = *static_cast<const SlotId*>(payload.Data);
            pending = [&rig, &state, slot, address, index] {
                state.topology.move(slot, address, index);
                state.applier.apply(state.topology, rig);
            };
        }

        /// @brief Makes a stage a drop target, draws the caret in the hovered gap and queues the drop.
        void draw_stage_target(RigController& rig, RigChainState& state, ImVec2 origin, const StageView& stage,
                               std::size_t stage_index, std::optional<SlotId>& created,
                               std::function<void()>& pending)
        {
            const StageGeometry& geometry = *stage.geometry;
            ImGui::PushID(static_cast<int>(stage_index));
            ImGui::SetCursorScreenPos(to_screen(origin, geometry.bounds.min));
            // Cards submitted after this target overlap it and keep their own hover.
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("stage", ImVec2(geometry.bounds.max.x - geometry.bounds.min.x,
                                                   geometry.bounds.max.y - geometry.bounds.min.y));
            if (ImGui::BeginDragDropTarget())
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const std::optional<std::size_t> gap =
                    nearest_gap(geometry, Vec2{ mouse.x - origin.x, mouse.y - origin.y });
                const ImGuiDragDropFlags flags =
                    ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
                const ImGuiPayload* gear = ImGui::AcceptDragDropPayload("GEAR_DESCRIPTOR_ID", flags);
                const ImGuiPayload* moved =
                    gear == nullptr ? ImGui::AcceptDragDropPayload("RACK_SLOT_ID", flags) : nullptr;
                const ImGuiPayload* payload = gear != nullptr ? gear : moved;
                if (payload != nullptr && gap.has_value())
                {
                    const Rect& rect = geometry.gaps[*gap];
                    const float caret_x = origin.x + (rect.min.x + rect.max.x) / 2.0f;
                    const float caret_y = origin.y + (rect.min.y + rect.max.y) / 2.0f;
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImVec2(caret_x - kCaretWidth / 2.0f, caret_y - kCaretHeight / 2.0f),
                        ImVec2(caret_x + kCaretWidth / 2.0f, caret_y + kCaretHeight / 2.0f),
                        to_imu32(palette().accent), kCaretWidth);
                    if (payload->IsDelivery())
                    {
                        queue_drop(rig, state, created, *payload, gear != nullptr, stage.address, *gap, pending);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }

        /// @brief Draws a circular IN or OUT terminal.
        void draw_terminal(ImDrawList* draw, ImVec2 origin, const Rect& rect, const char* label)
        {
            const ImVec2 center = to_screen(origin, Vec2{ (rect.min.x + rect.max.x) / 2.0f,
                                                          (rect.min.y + rect.max.y) / 2.0f });
            const float radius = (rect.max.x - rect.min.x) / 2.0f;
            draw->AddCircleFilled(center, radius, to_imu32(kIoFill));
            draw->AddCircle(center, radius, to_imu32(kCardBorder));
            const ImVec2 size = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(center.x - size.x / 2.0f, center.y - size.y / 2.0f),
                          to_imu32(palette().muted_text), label);
        }

        /// @brief Draws a splitter or mixer glyph as a teal box around the active chain type's icon.
        void draw_glyph(ImDrawList* draw, ImVec2 origin, const Rect& rect, ChainType type)
        {
            const ImVec2 top_left = to_screen(origin, rect.min);
            const ImVec2 bottom_right = to_screen(origin, rect.max);
            draw->AddRectFilled(top_left, bottom_right, to_imu32(kTealSoft), 8.0f);
            draw->AddRect(top_left, bottom_right, to_imu32(kTeal), 8.0f, 0, 1.5f);
            const ImVec2 center((top_left.x + bottom_right.x) / 2.0f, (top_left.y + bottom_right.y) / 2.0f);
            draw_chain_icon(draw, type, center, kIconSize, to_imu32(kTeal));
        }

        /// @brief Draws a branch's number badge above where its cable enters the row.
        void draw_badge(ImDrawList* draw, ImVec2 origin, const StageGeometry& row, std::size_t number)
        {
            const ImVec2 center(origin.x + row.bounds.min.x - kBadgeRadius - 4.0f,
                                origin.y + (row.bounds.min.y + row.bounds.max.y) / 2.0f - kBadgeRadius - 6.0f);
            draw->AddCircleFilled(center, kBadgeRadius, to_imu32(kTeal));
            char text[8];
            std::snprintf(text, sizeof(text), "%zu", number);
            const ImVec2 size = ImGui::CalcTextSize(text);
            draw->AddText(ImVec2(center.x - size.x / 2.0f, center.y - size.y / 2.0f), to_imu32(kCardFill), text);
        }

        /// @brief Lays out the topology's stages for the active chain type.
        SplitGeometry layout_topology(const RigTopology& topology)
        {
            const ChainType type = topology.chain_type();
            const std::size_t paths = RigTopology::branch_count(type);
            const std::size_t post = topology.stage(StageAddress{ StageKind::Post, 0 }).size();

            // One path draws as a straight run, which is what layout_split builds when no branch
            // sizes are given; the main path takes the geometry that run occupies.
            if (paths == 1)
            {
                return layout_split(topology.stage(StageAddress{ StageKind::Branch, 0 }).size(), 0,
                                    {}, 0, SplitMetrics{});
            }

            std::vector<std::size_t> branch_sizes;
            for (std::size_t i = 0; i < paths; ++i)
            {
                branch_sizes.push_back(topology.stage(StageAddress{ StageKind::Branch, i }).size());
            }
            return layout_split(0, 0, branch_sizes, post, SplitMetrics{});
        }

        /// @brief Draws the chain canvas and returns the one edit this frame's interaction queued.
        std::function<void()> draw_canvas(RigController& rig, RigChainState& state, std::optional<SlotId>& created)
        {
            const SplitGeometry geometry = layout_topology(state.topology);
            const ChainType type = state.topology.chain_type();
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 origin(cursor.x + kCanvasPadding, cursor.y + kCanvasPadding);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            std::function<void()> pending;

            // Cables go down first so glyphs and cards sit on top of them.
            std::vector<ImVec2> points;
            for (const std::vector<Vec2>& cable : geometry.cables)
            {
                points.clear();
                for (Vec2 point : cable)
                {
                    points.push_back(to_screen(origin, point));
                }
                draw->AddPolyline(points.data(), static_cast<int>(points.size()), to_imu32(kTeal), ImDrawFlags_None,
                                  kCableThickness);
            }

            const std::vector<StageView> stages = collect_stages(type, geometry);
            for (std::size_t s = 0; s < stages.size(); ++s)
            {
                const StageView& stage = stages[s];
                draw_stage_wires(draw, origin, stage);
                draw_stage_target(rig, state, origin, stage, s, created, pending);
                const std::vector<SlotId>& slots = state.topology.stage(stage.address);
                for (std::size_t n = 0; n < slots.size() && n < stage.geometry->nodes.size(); ++n)
                {
                    const Rect& rect = stage.geometry->nodes[n];
                    draw_node(rig, state, slots[n], to_screen(origin, rect.min), to_screen(origin, rect.max), pending);
                }
                // A badge numbers a path against its siblings; one path has none to be told from.
                if (stage.address.kind == StageKind::Branch && stages.size() > 1)
                {
                    draw_badge(draw, origin, *stage.geometry, stage.address.branch + 1);
                }
            }

            draw_terminal(draw, origin, geometry.input, "IN");
            draw_terminal(draw, origin, geometry.output, "OUT");
            if (geometry.splitter.has_value())
            {
                draw_glyph(draw, origin, *geometry.splitter, type);
            }
            if (geometry.mixer.has_value())
            {
                draw_glyph(draw, origin, *geometry.mixer, type);
            }

            ImGui::SetCursorScreenPos(cursor);
            ImGui::Dummy(ImVec2(geometry.size.x + kCanvasPadding * 2.0f, geometry.size.y + kCanvasPadding * 2.0f));
            return pending;
        }
    }

    void DrawRigChainView(RigController& rig, RigChainState& state, std::optional<SlotId>& created)
    {
        created = std::nullopt;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kRailPadding, kRailPadding));
        ImGui::BeginChild("##chain_rail", ImVec2(kButtonSize + kRailPadding * 2.0f, 0.0f),
                          ImGuiChildFlags_AlwaysUseWindowPadding);
        draw_rail(rig, state);
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();
        ImGui::BeginChild("##chain_canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        // The queued edit runs after the canvas, so no stage list changes while it is iterated.
        const std::function<void()> pending = draw_canvas(rig, state, created);
        ImGui::EndChild();
        if (pending)
        {
            run_edit(state, pending);
        }

        if (!slot_selection::still_valid(state.selected, rig.slot_order()))
        {
            state.selected = std::nullopt;
        }
    }
}
