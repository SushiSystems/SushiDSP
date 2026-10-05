/****************************************************************/
/* ArrangementView.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ArrangementView.cpp
 * @brief Implements the arrangement view and the route a dropped file takes onto a lane.
 * @author Mustafa Garip
 */

#include "ui/ArrangementView.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

#include "AudioImportCache.hpp"

#include "ui/ArrangementLayout.hpp"
#include "ui/Divider.hpp"
#include "ui/MusicalTimeText.hpp"
#include "ui/TextField.hpp"
#include "ui/Theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief How many lanes the view lays out, one per playback track.
        constexpr std::size_t kLaneCount = SushiDSP::TrackPlayer::kMaxTracks;

        /// @brief Width of the colour strip down a lane header's leading edge, in pixels.
        constexpr float kColorStripWidth = 5.0f;

        /// @brief Where a lane header's controls start, clear of the colour strip, in pixels.
        constexpr float kHeaderInset = 11.0f;

        /// @brief Characters a lane's name box holds, terminator included.
        constexpr std::size_t kNameCapacity = 64;

        /// @brief The header column's narrowest and the grid's narrowest width, in pixels.
        constexpr DividerLimits kHeaderDividerLimits{ 120.0f, 160.0f };

        /// @brief The width of the divider strip between the header column and the grid.
        constexpr float kHeaderDividerThickness = 14.0f;

        /// @brief The height of the bar ruler above the lanes, in pixels.
        constexpr float kRulerHeight = 26.0f;

        /// @brief The height of one lane, in pixels.
        constexpr float kLaneHeight = 66.0f;

        /// @brief The gap between two lanes, in pixels.
        constexpr float kLaneGap = 2.0f;

        /// @brief The narrowest zoom the view offers, in pixels a frame covers.
        constexpr double kMinimumZoom = 0.00002;

        /// @brief The widest zoom the view offers, in pixels a frame covers.
        constexpr double kMaximumZoom = 0.05;

        /// @brief What one wheel notch multiplies the zoom by.
        constexpr double kZoomStep = 1.25;

        /// @brief Index meaning no clip is being dragged.
        constexpr std::size_t kNoClip = static_cast<std::size_t>(-1);

        /// @brief Names the clip the pointer is dragging and the frame it started at.
        struct ClipDrag
        {
            std::size_t   track = 0;            ///< Holds the lane the clip sits on.
            std::size_t   clip_index = kNoClip; ///< Holds the clip's index in start order.
            std::uint64_t origin_frame = 0;     ///< Holds the frame the drag started at.
        };

        /// @brief Reports whether a drag names a clip.
        /// @param drag The drag to test.
        bool drag_is_live(const ClipDrag& drag)
        {
            return drag.clip_index != kNoClip;
        }

        /// @brief Names a file by its stem alone, so a clip is labelled with what the take is
        /// called rather than where it lives.
        /// @param path The file's path.
        /// @return The stem, or "clip" for a path with none.
        std::string file_stem(const std::string& path)
        {
            const std::string stem = std::filesystem::path(path).stem().string();
            return stem.empty() ? std::string("clip") : stem;
        }

        /// @brief Moves the scroll so the frame under a canvas x stays under it while the zoom
        /// changes.
        /// @param scale In/out: the scale being zoomed.
        /// @param canvas_x The x the zoom pivots about.
        /// @param factor What the zoom is multiplied by.
        void zoom_about(TimelineScale& scale, float canvas_x, double factor)
        {
            const std::uint64_t anchor = scale.frame_of_x(canvas_x);
            const double        zoom = std::clamp(scale.pixels_per_frame() * factor, kMinimumZoom,
                                                  kMaximumZoom);

            scale.set_pixels_per_frame(zoom);

            const double origin =
                static_cast<double>(anchor) - static_cast<double>(canvas_x) / zoom;
            scale.set_scroll_frames(origin > 0.0 ? static_cast<std::uint64_t>(origin) : 0u);
        }

        /// @brief Scrolls the shared origin by a pixel distance, clamped at frame zero.
        /// @param scale In/out: the scale being scrolled.
        /// @param pixels How far to scroll, negative towards the timeline's front.
        void scroll_by_pixels(TimelineScale& scale, float pixels)
        {
            const double moved = static_cast<double>(scale.scroll_frames())
                               + static_cast<double>(pixels) / scale.pixels_per_frame();
            scale.set_scroll_frames(moved > 0.0 ? static_cast<std::uint64_t>(moved) : 0u);
        }

        /// @brief Draws the toolbar above the ruler: the zoom, the snap and the clip counter.
        /// @param rig The rig whose lanes the counter reads.
        /// @param scale In/out: the zoom the buttons change.
        /// @param snap In/out: the snap the combo chooses.
        void draw_toolbar(RigController& rig, TimelineScale& scale, SnapMode& snap)
        {
            if (ImGui::SmallButton("-"))
            {
                zoom_about(scale, 0.0f, 1.0 / kZoomStep);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+"))
            {
                zoom_about(scale, 0.0f, kZoomStep);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("|<"))
            {
                scale.set_scroll_frames(0u);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(104.0f);
            int chosen = static_cast<int>(snap);
            if (ImGui::Combo("##snap", &chosen, "Snap off\0Snap beat\0Snap bar\0"))
            {
                snap = static_cast<SnapMode>(chosen);
            }

            std::size_t placed = 0;
            for (std::size_t lane = 0; lane < rig.track_count(); ++lane)
            {
                placed += rig.clip_count(lane);
            }

            ImGui::SameLine();
            ImGui::TextColored(to_imvec4(palette().muted_text), "%zu clips", placed);
        }

        /// @brief Draws the line naming what the import worker is decoding, so a song that takes
        /// seconds to convert says so instead of appearing to do nothing.
        /// @param imports The queue whose progress is reported.
        void draw_import_notice(const SushiDSP::host::AudioImportQueue& imports)
        {
            const std::string decoding = imports.decoding();
            if (decoding.empty())
            {
                return;
            }

            const std::size_t counted = imports.pending();
            const std::size_t queued = counted > 0u ? counted - 1u : 0u;
            const std::string name = std::filesystem::path(decoding).filename().string();
            if (queued == 0)
            {
                ImGui::TextColored(to_imvec4(palette().accent), "Decoding %s...", name.c_str());
            }
            else
            {
                ImGui::TextColored(to_imvec4(palette().accent), "Decoding %s... (%zu waiting)",
                                   name.c_str(), queued);
            }
        }

        /// @brief Draws one lane's header strip: the name, the level, the arm, mute and solo
        /// buttons, the pan and the re-amp button.
        /// @param rig The rig whose player and re-amp leg the strip drives.
        /// @param index The lane.
        /// @param origin The strip's top-left in screen pixels.
        /// @param header_width The header strip's width.
        /// @param names In/out: the name boxes' character buffers.
        /// @param error_out In/out: the last refusal.
        void draw_lane_header(RigController& rig, std::size_t index, ImVec2 origin,
                              float header_width,
                              std::array<std::array<char, kNameCapacity>, kLaneCount>& names,
                              std::optional<std::size_t>& remove_request, std::string& error_out)
        {
            SushiDSP::TrackPlayer& player = rig.player();
            ImDrawList*            draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(origin, ImVec2(origin.x + header_width, origin.y + kLaneHeight),
                                to_imu32(rgba(38, 38, 44)), 4.0f);

            // Strip click: palette; header right-click: removal; see apps/host_gui/ui/README.md.
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("color", ImVec2(kColorStripWidth, kLaneHeight));
            const bool color_hovered = ImGui::IsItemHovered();
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + kColorStripWidth, origin.y + kLaneHeight),
                                rig.track_color(index), 4.0f);
            if (color_hovered)
            {
                ImGui::SetTooltip("Track colour; right-click the lane to remove it");
            }
            if (ImGui::IsItemClicked())
            {
                ImGui::OpenPopup("palette");
            }
            if (ImGui::BeginPopup("palette"))
            {
                for (std::size_t entry = 0; entry < SessionTimeline::palette_size(); ++entry)
                {
                    if (entry != 0)
                    {
                        ImGui::SameLine(0.0f, 4.0f);
                    }
                    ImGui::PushID(static_cast<int>(entry));
                    const ImVec2 swatch = ImGui::GetCursorScreenPos();
                    if (ImGui::InvisibleButton("swatch", ImVec2(18.0f, 18.0f)))
                    {
                        rig.set_track_color(index, SessionTimeline::palette_color(entry));
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        swatch, ImVec2(swatch.x + 18.0f, swatch.y + 18.0f),
                        SessionTimeline::palette_color(entry), 3.0f);
                    ImGui::PopID();
                }
                ImGui::EndPopup();
            }

            ImGui::SetCursorScreenPos(ImVec2(origin.x + kHeaderInset, origin.y + 5.0f));
            ImGui::SetNextItemWidth(100.0f);
            if (TextField("##name", names[index].data(), kNameCapacity, rig.track_name(index)))
            {
                rig.set_track_name(index, names[index].data());
            }

            const bool is_reamp_source =
                rig.reamp_track().has_value() && *rig.reamp_track() == index;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + kHeaderInset + 106.0f, origin.y + 6.0f));
            if (ImGui::SmallButton(is_reamp_source ? "RE-AMP" : "re-amp"))
            {
                if (is_reamp_source)
                {
                    rig.clear_reamp_track();
                    rig.set_input_source(RigInputSource::LiveInput);
                }
                else if (rig.set_reamp_track(index, error_out))
                {
                    rig.set_input_source(RigInputSource::Track);
                }
            }

            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::SmallButton("x"))
            {
                remove_request = index;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove this track");
            }

            ImGui::SetCursorScreenPos(ImVec2(origin.x + kHeaderInset, origin.y + 26.0f));
            ImGui::SetNextItemWidth(184.0f);
            float level = player.gain(index);
            if (ImGui::SliderFloat("##level", &level, 0.0f, 2.0f, "%.2f"))
            {
                player.set_gain(index, level);
            }

            ImGui::SetCursorScreenPos(ImVec2(origin.x + kHeaderInset, origin.y + 46.0f));
            const bool armed = rig.record_track().has_value() && *rig.record_track() == index;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(armed ? rgba(196, 54, 46) : rgba(52, 52, 60)));
            if (ImGui::SmallButton("R"))
            {
                rig.set_record_track(armed ? std::optional<std::size_t>()
                                           : std::optional<std::size_t>(index));
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 4.0f);
            const bool muted = player.muted(index);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(muted ? rgba(184, 148, 40) : rgba(52, 52, 60)));
            if (ImGui::SmallButton("M"))
            {
                player.set_mute(index, !muted);
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 4.0f);
            const bool soloed = player.soloed(index);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(soloed ? rgba(64, 152, 196) : rgba(52, 52, 60)));
            if (ImGui::SmallButton("S"))
            {
                player.set_solo(index, !soloed);
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetNextItemWidth(92.0f);
            float pan = player.pan(index);
            if (ImGui::SliderFloat("##pan", &pan, -1.0f, 1.0f, "pan %.2f"))
            {
                player.set_pan(index, pan);
            }
        }

        /// @brief Draws one lane's clips and runs their drag and their right-click menu.
        /// @param rig The rig whose clips the lane holds.
        /// @param scale The zoom and scroll every lane shares.
        /// @param snap What a dragged clip's frame is rounded to.
        /// @param origin The canvas's top-left in screen pixels.
        /// @param canvas_width The canvas width in pixels.
        /// @param drag In/out: the clip the pointer is dragging, if any.
        /// @param error_out In/out: the last refusal.
        void draw_lane_clips(RigController& rig, const TimelineScale& scale, SnapMode snap,
                             std::size_t index, ImVec2 origin, float canvas_width, ClipDrag& drag,
                             std::string& error_out)
        {
            ImDrawList*                   draw = ImGui::GetWindowDrawList();
            const SushiDSP::MusicalClock& clock = rig.musical_clock();
            const std::size_t             count = rig.clip_count(index);

            for (std::size_t clip_index = 0; clip_index < count; ++clip_index)
            {
                const RigController::ClipPlacement placement = rig.clip(index, clip_index);
                const bool dragged = drag_is_live(drag) && drag.track == index
                                  && drag.clip_index == clip_index;
                const std::uint64_t start =
                    dragged ? dragged_start_frame(scale, snap, clock, drag.origin_frame,
                                                  ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x)
                            : placement.start_frame;
                const ClipSpan span = clip_span(scale, start, placement.length_frames);

                if (span.right < 0.0f || span.left > canvas_width)
                {
                    continue;
                }

                const float  left = std::max(span.left, 0.0f);
                const float  right = std::min(std::max(span.right, left + 2.0f), canvas_width);
                const ImVec2 top_left(origin.x + left, origin.y + 3.0f);
                const ImVec2 bottom_right(origin.x + right, origin.y + kLaneHeight - 3.0f);

                draw->AddRectFilled(top_left, bottom_right,
                                    to_imu32(dragged ? rgba(96, 132, 176) : rgba(64, 96, 136)),
                                    4.0f);
                draw->AddRect(top_left, bottom_right, to_imu32(palette().accent), 4.0f, 0, 1.0f);

                const std::string label =
                    file_stem(placement.path) + "  " + bar_beat_text(start, clock);
                draw->PushClipRect(top_left, bottom_right, true);
                draw->AddText(ImVec2(top_left.x + 5.0f, top_left.y + 4.0f),
                              to_imu32(palette().text), label.c_str());
                draw->PopClipRect();

                // A clip's id is its start frame, unique on a lane; see apps/host_gui/ui/README.md.
                std::array<char, 32> clip_id{};
                std::snprintf(clip_id.data(), clip_id.size(), "clip%llu",
                              static_cast<unsigned long long>(placement.start_frame));
                ImGui::PushID(clip_id.data());
                ImGui::SetCursorScreenPos(top_left);
                ImGui::InvisibleButton("clip",
                                       ImVec2(std::max(bottom_right.x - top_left.x, 2.0f),
                                              bottom_right.y - top_left.y));

                if (ImGui::IsItemActivated())
                {
                    drag.track = index;
                    drag.clip_index = clip_index;
                    drag.origin_frame = placement.start_frame;
                }

                if (dragged && ImGui::IsItemDeactivated())
                {
                    if (start != placement.start_frame)
                    {
                        rig.move_clip(index, clip_index, start, error_out);
                    }
                    drag.clip_index = kNoClip;
                }

                if (ImGui::BeginPopupContextItem("clip_menu"))
                {
                    ImGui::TextColored(to_imvec4(palette().muted_text), "%s", label.c_str());
                    if (ImGui::MenuItem("Remove clip"))
                    {
                        rig.remove_clip(index, clip_index, error_out);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }

        /// @brief Draws the bar numbers, the bar lines and the beat lines over the ruler and
        /// down every lane.
        /// @param lines The grid the canvas shows.
        /// @param canvas_x The canvas's left edge in screen pixels.
        /// @param top The ruler's top in screen pixels.
        /// @param bottom The last lane's bottom in screen pixels.
        void draw_grid(const std::vector<GridLine>& lines, float canvas_x, float top, float bottom)
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();

            for (const GridLine& line : lines)
            {
                const float x = canvas_x + line.x;
                draw->AddLine(ImVec2(x, line.is_bar ? top : top + kRulerHeight * 0.55f),
                              ImVec2(x, bottom),
                              to_imu32(line.is_bar ? rgba(84, 84, 96) : rgba(46, 46, 54)));

                if (!line.is_bar)
                {
                    continue;
                }

                std::array<char, 24> text{};
                std::snprintf(text.data(), text.size(), "%llu",
                              static_cast<unsigned long long>(line.bar));
                draw->AddText(ImVec2(x + 4.0f, top + 4.0f), to_imu32(palette().muted_text),
                              text.data());
            }
        }
    }

    bool route_file_drop(RigController& rig, const TimelineScale& scale, SnapMode snap,
                         const std::string& path, ImVec2 canvas_position, std::size_t track,
                         SushiDSP::host::AudioImportQueue& imports, std::string& error_out)
    {
        const std::uint64_t start =
            dropped_frame(scale, snap, rig.musical_clock(), canvas_position.x);
        if (SushiDSP::host::AudioImportCache::can_import(path))
        {
            imports.request(path, track, start, rig.sample_rate());
            error_out.clear();
            return true;
        }
        return rig.place_clip(track, path, start, error_out);
    }

    void DrawArrangementView(RigController& rig, TimelineScale& scale, SnapMode& snap,
                             SushiDSP::host::AudioImportQueue& imports,
                             std::string& arrangement_error, LaneGeometry& geometry_out,
                             float& header_width)
    {
        static std::array<std::array<char, kNameCapacity>, kLaneCount> names{};
        static ClipDrag                                               drag;
        static std::optional<std::size_t>                             pending_removal;
        std::optional<std::size_t>                                    remove_request;

        draw_toolbar(rig, scale, snap);

        const SushiDSP::MusicalClock& clock = rig.musical_clock();
        SushiDSP::Transport&          transport = rig.transport();
        ImDrawList*                   draw = ImGui::GetWindowDrawList();
        const ImVec2                  origin = ImGui::GetCursorScreenPos();
        const float                   total_width = ImGui::GetContentRegionAvail().x;
        const float                   canvas_x = origin.x + header_width + kHeaderDividerThickness;
        const float                   canvas_width =
            std::max(total_width - header_width - kHeaderDividerThickness, 32.0f);
        const float lanes_top = origin.y + kRulerHeight;
        const float lanes_bottom =
            lanes_top + static_cast<float>(rig.track_count()) * (kLaneHeight + kLaneGap);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::SetCursorScreenPos(ImVec2(origin.x + header_width, origin.y));
        ImGui::BeginChild("##header_divider", ImVec2(kHeaderDividerThickness, lanes_bottom - origin.y),
                          false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawDivider("header_divider", DividerAxis::Vertical, header_width, total_width,
                   kHeaderDividerLimits, false);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        geometry_out.canvas_left = canvas_x;
        geometry_out.canvas_top = lanes_top;
        geometry_out.canvas_width = canvas_width;
        geometry_out.lane_height = kLaneHeight;
        geometry_out.lane_stride = kLaneHeight + kLaneGap;
        geometry_out.lane_count = rig.track_count();

        draw->AddRectFilled(ImVec2(canvas_x, origin.y),
                            ImVec2(canvas_x + canvas_width, origin.y + kRulerHeight),
                            to_imu32(rgba(30, 30, 36)), 3.0f);
        draw->AddRectFilled(ImVec2(canvas_x, lanes_top),
                            ImVec2(canvas_x + canvas_width, lanes_bottom),
                            to_imu32(rgba(22, 22, 26)));
        draw_grid(grid_lines(scale, clock, canvas_width), canvas_x, origin.y, lanes_bottom);

        ImGui::SetCursorScreenPos(ImVec2(canvas_x, origin.y));
        ImGui::InvisibleButton("ruler", ImVec2(canvas_width, kRulerHeight));
        if (ImGui::IsItemActive())
        {
            const float pointer = ImGui::GetIO().MousePos.x - canvas_x;
            transport.seek(scale.snap(scale.frame_of_x(pointer), snap, clock));
        }

        const std::size_t lane_count = rig.track_count();
        for (std::size_t index = 0; index < lane_count; ++index)
        {
            const float lane_y = lanes_top + static_cast<float>(index) * (kLaneHeight + kLaneGap);

            ImGui::PushID(static_cast<int>(index));
            draw_lane_header(rig, index, ImVec2(origin.x, lane_y), header_width, names,
                             remove_request, arrangement_error);

            ImGui::SetCursorScreenPos(ImVec2(canvas_x, lane_y));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("canvas", ImVec2(canvas_width, kLaneHeight));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TAKE_FILE_PATH"))
                {
                    const std::string path(static_cast<const char*>(payload->Data));
                    const ImVec2      landed(ImGui::GetIO().MousePos.x - canvas_x, 0.0f);
                    if (route_file_drop(rig, scale, snap, path, landed, index, imports,
                                        arrangement_error))
                    {
                        arrangement_error.clear();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            draw_lane_clips(rig, scale, snap, index, ImVec2(canvas_x, lane_y), canvas_width, drag,
                            arrangement_error);
            ImGui::PopID();

            draw->AddLine(ImVec2(canvas_x, lane_y + kLaneHeight),
                          ImVec2(canvas_x + canvas_width, lane_y + kLaneHeight),
                          to_imu32(rgba(46, 46, 54)));
        }

        // Removing a track asks about its files every time; see apps/host_gui/ui/README.md.
        ImGui::SetCursorScreenPos(ImVec2(origin.x,
                                         lanes_top + static_cast<float>(lane_count)
                                             * (kLaneHeight + kLaneGap) + 4.0f));
        if (ImGui::SmallButton("+ Add track"))
        {
            std::string error;
            if (!rig.add_track(error))
            {
                arrangement_error = error;
            }
        }

        if (remove_request.has_value())
        {
            pending_removal = remove_request;
            ImGui::OpenPopup("Remove track");
        }
        if (ImGui::BeginPopupModal("Remove track", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Remove the lane, or the lane and the files it reads?");
            std::string error;
            if (ImGui::Button("Lane only"))
            {
                if (pending_removal.has_value()
                    && !rig.remove_track(*pending_removal, error))
                {
                    arrangement_error = error;
                }
                pending_removal.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Lane and files"))
            {
                if (pending_removal.has_value()
                    && !rig.remove_track_and_files(*pending_removal, error))
                {
                    arrangement_error = error;
                }
                pending_removal.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                pending_removal.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        const float playhead_x = canvas_x + scale.x_of(transport.position_frames());
        if (playhead_x >= canvas_x && playhead_x <= canvas_x + canvas_width)
        {
            draw->AddLine(ImVec2(playhead_x, origin.y), ImVec2(playhead_x, lanes_bottom),
                          to_imu32(rgba(220, 72, 60)), 1.6f);
            draw->AddTriangleFilled(ImVec2(playhead_x - 5.0f, origin.y),
                                    ImVec2(playhead_x + 5.0f, origin.y),
                                    ImVec2(playhead_x, origin.y + 8.0f),
                                    to_imu32(rgba(220, 72, 60)));
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.0f)
        {
            const float pointer = io.MousePos.x - canvas_x;
            if (io.KeyCtrl)
            {
                zoom_about(scale, pointer, io.MouseWheel > 0.0f ? kZoomStep : 1.0 / kZoomStep);
            }
            else
            {
                scroll_by_pixels(scale, -io.MouseWheel * canvas_width * 0.12f);
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(origin.x, lanes_bottom + 4.0f));
        draw_import_notice(imports);
        if (!arrangement_error.empty())
        {
            ImGui::TextColored(to_imvec4(rgba(220, 72, 60)), "%s", arrangement_error.c_str());
        }
    }
}
