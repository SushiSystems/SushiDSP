/**************************************************************************/
/* arrangement_view.cpp                                                   */
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
 * @file arrangement_view.cpp
 * @brief Implements the arrangement view and the route a dropped file takes onto a lane.
 * @author Mustafa Garip
 */

#include "ui/arrangement_view.hpp"

#include "session_file.hpp"

#include <exception>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

#include "audio_import_cache.hpp"

#include "ui/arrangement_layout.hpp"
#include "ui/divider.hpp"
#include "ui/musical_time_text.hpp"
#include "ui/text_field.hpp"
#include "ui/theme.hpp"

namespace ui
{
    namespace
    {
        /// @brief How many lanes the view lays out, one per playback track.
        constexpr std::size_t K_LANE_COUNT = SushiDSP::TrackPlayer::K_MAX_TRACKS;

        /// @brief Width of the colour strip down a lane header's leading edge, in pixels.
        constexpr float K_COLOR_STRIP_WIDTH = 5.0f;

        /// @brief Where a lane header's controls start, clear of the colour strip, in pixels.
        constexpr float K_HEADER_INSET = 11.0f;

        /// @brief Characters a lane's name box holds, terminator included.
        constexpr std::size_t K_NAME_CAPACITY = 64;

        /// @brief The header column's narrowest and the grid's narrowest width, in pixels.
        constexpr DividerLimits K_HEADER_DIVIDER_LIMITS{120.0f, 160.0f};

        /// @brief The width of the divider strip between the header column and the grid.
        constexpr float K_HEADER_DIVIDER_THICKNESS = 14.0f;

        /// @brief The height of the bar ruler above the lanes, in pixels.
        constexpr float K_RULER_HEIGHT = 26.0f;

        /// @brief Holds the gap between two rows of a track's header, in pixels.
        constexpr float K_HEADER_ROW_GAP = 4.0f;

        /// @brief Holds the space above the first and under the last row of a header, in pixels.
        constexpr float K_HEADER_PADDING = 6.0f;

        /// @brief Returns a lane's height: three rows of controls at the font in use.
        float lane_height()
        {
            return 3.0f * ImGui::GetFrameHeight() + 2.0f * K_HEADER_ROW_GAP +
                   2.0f * K_HEADER_PADDING;
        }

        /// @brief Returns the top of one of a header's three rows, from the header's own top.
        float header_row_y(int row)
        {
            return K_HEADER_PADDING +
                   static_cast<float>(row) * (ImGui::GetFrameHeight() + K_HEADER_ROW_GAP);
        }

        /// @brief Returns the width a button of one label takes.
        float button_width(const char* label)
        {
            return ImGui::CalcTextSize(label).x + 2.0f * ImGui::GetStyle().FramePadding.x;
        }

        /// @brief The gap between two lanes, in pixels.
        constexpr float K_LANE_GAP = 2.0f;

        /// @brief The narrowest zoom the view offers, in pixels a frame covers.
        constexpr double K_MINIMUM_ZOOM = 0.00002;

        /// @brief The widest zoom the view offers, in pixels a frame covers.
        constexpr double K_MAXIMUM_ZOOM = 0.05;

        /// @brief What one wheel notch multiplies the zoom by.
        constexpr double K_ZOOM_STEP = 1.25;

        /// @brief Index meaning no clip is being dragged.
        constexpr std::size_t K_NO_CLIP = static_cast<std::size_t>(-1);

        /// @brief Names the clip the pointer is dragging and the frame it started at.
        struct ClipDrag
        {
                std::size_t track = 0;              ///< Holds the lane the clip sits on.
                std::size_t clip_index = K_NO_CLIP; ///< Holds the clip's index in start order.
                std::uint64_t origin_frame = 0;     ///< Holds the frame the drag started at.
        };

        /// @brief Reports whether a drag names a clip.
        /// @param drag The drag to test.
        bool drag_is_live(const ClipDrag& drag)
        {
            return drag.clip_index != K_NO_CLIP;
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
            const double zoom =
                std::clamp(scale.pixels_per_frame() * factor, K_MINIMUM_ZOOM, K_MAXIMUM_ZOOM);

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
            const double moved = static_cast<double>(scale.scroll_frames()) +
                                 static_cast<double>(pixels) / scale.pixels_per_frame();
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
                zoom_about(scale, 0.0f, 1.0 / K_ZOOM_STEP);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+"))
            {
                zoom_about(scale, 0.0f, K_ZOOM_STEP);
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

        /// @brief Draws the popup a track's rig button opens: the rig's preset file and the
        /// track's own file, each loaded and saved by path.
        void draw_track_files(Session& session, std::size_t index, std::string& error_out)
        {
            static char rig_path[256] = "rig.json";
            static char track_path[256] = "track.sdtrack";
            if (!ImGui::BeginPopup("track_files"))
            {
                return;
            }
            ImGui::TextUnformatted("This track's rig");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("##rig_path", rig_path, sizeof(rig_path));
            if (ImGui::Button("Load rig"))
            {
                if (load_track_rig(session, index, rig_path, error_out))
                {
                    error_out.clear();
                    session.select(index);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save rig"))
            {
                if (save_track_rig(session, index, rig_path, error_out))
                {
                    error_out.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            ImGui::TextUnformatted("The whole track: rig, strip and clips");
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText("##track_path", track_path, sizeof(track_path));
            if (ImGui::Button("Save track"))
            {
                try
                {
                    if (write_text_file(track_path, track_to_json(session, index), error_out))
                    {
                        error_out.clear();
                    }
                }
                catch (const std::exception& refusal)
                {
                    error_out = refusal.what();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Load as a new track"))
            {
                std::string text;
                if (read_text_file(track_path, text, error_out))
                {
                    track_from_json(session, text, error_out);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        /// @brief Draws one track's header: the name, the rig button that selects the track and
        /// opens its files, the rig bypass, the level, the arm, mute and solo buttons and the pan.
        /// @param session The session whose track the header drives.
        /// @param index The track.
        /// @param origin The strip's top-left in screen pixels.
        /// @param header_width The header strip's width.
        /// @param names In/out: the name boxes' character buffers.
        /// @param error_out In/out: the last refusal.
        void draw_lane_header(Session& session, std::size_t index, ImVec2 origin,
                              float header_width,
                              std::array<std::array<char, K_NAME_CAPACITY>, K_LANE_COUNT>& names,
                              std::optional<std::size_t>& remove_request, std::string& error_out)
        {
            RigController& rig = session.selected_rig();
            TrackStrip strip = session.strip(index);
            bool strip_changed = false;
            const bool is_selected = session.selected() == index;
            ImDrawList* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(origin, ImVec2(origin.x + header_width, origin.y + lane_height()),
                                to_imu32(is_selected ? rgba(52, 52, 62) : rgba(38, 38, 44)), 4.0f);
            if (is_selected)
            {
                draw->AddRect(origin, ImVec2(origin.x + header_width, origin.y + lane_height()),
                              to_imu32(rgba(214, 168, 74)), 4.0f);
            }

            // Strip click: palette; header right-click: removal; see applications/host_gui/ui/README.md.
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("color", ImVec2(K_COLOR_STRIP_WIDTH, lane_height()));
            const bool color_hovered = ImGui::IsItemHovered();
            draw->AddRectFilled(origin,
                                ImVec2(origin.x + K_COLOR_STRIP_WIDTH, origin.y + lane_height()),
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

            // Every row spans the header: the name takes what the three buttons leave.
            const float row_width = header_width - K_HEADER_INSET - 8.0f;
            const float buttons_width =
                button_width("rig") + button_width("byp") + button_width("x") + 3.0f * 4.0f;
            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + K_HEADER_INSET, origin.y + header_row_y(0)));
            ImGui::SetNextItemWidth(std::max(row_width - buttons_width, 40.0f));
            if (TextField("##name", names[index].data(), K_NAME_CAPACITY, rig.track_name(index)))
            {
                rig.set_track_name(index, names[index].data());
            }

            // The rig button selects the track, so the rig view shows this track's rig.
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(is_selected ? rgba(150, 118, 52) : rgba(52, 52, 60)));
            if (ImGui::Button("rig"))
            {
                session.select(index);
                ImGui::OpenPopup("track_files");
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                const std::string& preset = session.preset_path(index);
                ImGui::SetTooltip("Select this track and open its rig and track files.\n%s",
                                  preset.empty() ? "No preset loaded." : preset.c_str());
            }
            draw_track_files(session, index, error_out);

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, to_imvec4(strip.rig_bypassed ? rgba(120, 120, 130)
                                                                               : rgba(52, 52, 60)));
            if (ImGui::Button("byp"))
            {
                strip.rig_bypassed = !strip.rig_bypassed;
                strip_changed = true;
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Pass this track's audio by its rig, for a play-along track");
            }

            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::Button("x"))
            {
                remove_request = index;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Remove this track");
            }

            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + K_HEADER_INSET, origin.y + header_row_y(1)));
            ImGui::SetNextItemWidth(row_width);
            if (ImGui::SliderFloat("##level", &strip.level_db, -60.0f, 12.0f, "%+.1f dB"))
            {
                strip_changed = true;
            }

            ImGui::SetCursorScreenPos(
                ImVec2(origin.x + K_HEADER_INSET, origin.y + header_row_y(2)));
            const bool armed = rig.record_track().has_value() && *rig.record_track() == index;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(armed ? rgba(196, 54, 46) : rgba(52, 52, 60)));
            if (ImGui::Button("R"))
            {
                rig.set_record_track(armed ? std::optional<std::size_t>()
                                           : std::optional<std::size_t>(index));
                // What is played is heard through the rig of the track it is recorded onto.
                if (!armed)
                {
                    session.select(index);
                }
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(strip.muted ? rgba(184, 148, 40) : rgba(52, 52, 60)));
            if (ImGui::Button("M"))
            {
                strip.muted = !strip.muted;
                strip_changed = true;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  to_imvec4(strip.soloed ? rgba(64, 152, 196) : rgba(52, 52, 60)));
            if (ImGui::Button("S"))
            {
                strip.soloed = !strip.soloed;
                strip_changed = true;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetNextItemWidth(std::max(
                row_width - button_width("R") - button_width("M") - button_width("S") - 16.0f,
                40.0f));
            if (ImGui::SliderFloat("##pan", &strip.pan, -1.0f, 1.0f, "pan %.2f"))
            {
                strip_changed = true;
            }
            if (strip_changed)
            {
                session.set_strip(index, strip);
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
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const SushiDSP::MusicalClock& clock = rig.musical_clock();
            const std::size_t count = rig.clip_count(index);

            for (std::size_t clip_index = 0; clip_index < count; ++clip_index)
            {
                const RigController::ClipPlacement placement = rig.clip(index, clip_index);
                const bool dragged =
                    drag_is_live(drag) && drag.track == index && drag.clip_index == clip_index;
                const std::uint64_t start =
                    dragged ? dragged_start_frame(scale, snap, clock, drag.origin_frame,
                                                  ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x)
                            : placement.start_frame;
                const ClipSpan span = clip_span(scale, start, placement.length_frames);

                if (span.right < 0.0f || span.left > canvas_width)
                {
                    continue;
                }

                const float left = std::max(span.left, 0.0f);
                const float right = std::min(std::max(span.right, left + 2.0f), canvas_width);
                const ImVec2 top_left(origin.x + left, origin.y + 3.0f);
                const ImVec2 bottom_right(origin.x + right, origin.y + lane_height() - 3.0f);

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

                // A clip's id is its start frame, unique on a lane; see applications/host_gui/ui/README.md.
                std::array<char, 32> clip_id{};
                std::snprintf(clip_id.data(), clip_id.size(), "clip%llu",
                              static_cast<unsigned long long>(placement.start_frame));
                ImGui::PushID(clip_id.data());
                ImGui::SetCursorScreenPos(top_left);
                ImGui::InvisibleButton("clip", ImVec2(std::max(bottom_right.x - top_left.x, 2.0f),
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
                    drag.clip_index = K_NO_CLIP;
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
                draw->AddLine(ImVec2(x, line.is_bar ? top : top + K_RULER_HEIGHT * 0.55f),
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
    } // namespace

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

    void DrawArrangementView(Session& session, TimelineScale& scale, SnapMode& snap,
                             SushiDSP::host::AudioImportQueue& imports,
                             std::string& arrangement_error, LaneGeometry& geometry_out,
                             float& header_width)
    {
        static std::array<std::array<char, K_NAME_CAPACITY>, K_LANE_COUNT> names{};
        static ClipDrag drag;
        static std::optional<std::size_t> pending_removal;
        std::optional<std::size_t> remove_request;
        // Every track's rig forwards to the one timeline, so the selected one stands for it.
        RigController& rig = session.selected_rig();

        draw_toolbar(rig, scale, snap);

        const SushiDSP::MusicalClock& clock = rig.musical_clock();
        SushiDSP::Transport& transport = rig.transport();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float total_width = ImGui::GetContentRegionAvail().x;
        const float canvas_x = origin.x + header_width + K_HEADER_DIVIDER_THICKNESS;
        const float canvas_width =
            std::max(total_width - header_width - K_HEADER_DIVIDER_THICKNESS, 32.0f);
        const float lanes_top = origin.y + K_RULER_HEIGHT;
        const float lanes_bottom =
            lanes_top + static_cast<float>(rig.track_count()) * (lane_height() + K_LANE_GAP);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::SetCursorScreenPos(ImVec2(origin.x + header_width, origin.y));
        ImGui::BeginChild("##header_divider",
                          ImVec2(K_HEADER_DIVIDER_THICKNESS, lanes_bottom - origin.y), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawDivider("header_divider", DividerAxis::Vertical, header_width, total_width,
                    K_HEADER_DIVIDER_LIMITS, false);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        geometry_out.canvas_left = canvas_x;
        geometry_out.canvas_top = lanes_top;
        geometry_out.canvas_width = canvas_width;
        geometry_out.lane_height = lane_height();
        geometry_out.lane_stride = lane_height() + K_LANE_GAP;
        geometry_out.lane_count = rig.track_count();

        draw->AddRectFilled(ImVec2(canvas_x, origin.y),
                            ImVec2(canvas_x + canvas_width, origin.y + K_RULER_HEIGHT),
                            to_imu32(rgba(30, 30, 36)), 3.0f);
        draw->AddRectFilled(ImVec2(canvas_x, lanes_top),
                            ImVec2(canvas_x + canvas_width, lanes_bottom),
                            to_imu32(rgba(22, 22, 26)));
        draw_grid(grid_lines(scale, clock, canvas_width), canvas_x, origin.y, lanes_bottom);

        ImGui::SetCursorScreenPos(ImVec2(canvas_x, origin.y));
        ImGui::InvisibleButton("ruler", ImVec2(canvas_width, K_RULER_HEIGHT));
        if (ImGui::IsItemActive())
        {
            const float pointer = ImGui::GetIO().MousePos.x - canvas_x;
            transport.seek(scale.snap(scale.frame_of_x(pointer), snap, clock));
        }

        const std::size_t lane_count = rig.track_count();
        for (std::size_t index = 0; index < lane_count; ++index)
        {
            const float lane_y =
                lanes_top + static_cast<float>(index) * (lane_height() + K_LANE_GAP);

            ImGui::PushID(static_cast<int>(index));
            draw_lane_header(session, index, ImVec2(origin.x, lane_y), header_width, names,
                             remove_request, arrangement_error);

            ImGui::SetCursorScreenPos(ImVec2(canvas_x, lane_y));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton("canvas", ImVec2(canvas_width, lane_height()));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TAKE_FILE_PATH"))
                {
                    const std::string path(static_cast<const char*>(payload->Data));
                    const ImVec2 landed(ImGui::GetIO().MousePos.x - canvas_x, 0.0f);
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

            draw->AddLine(ImVec2(canvas_x, lane_y + lane_height()),
                          ImVec2(canvas_x + canvas_width, lane_y + lane_height()),
                          to_imu32(rgba(46, 46, 54)));
        }

        // Removing a track asks about its files every time; see applications/host_gui/ui/README.md.
        ImGui::SetCursorScreenPos(ImVec2(
            origin.x,
            lanes_top + static_cast<float>(lane_count) * (lane_height() + K_LANE_GAP) + 4.0f));
        if (ImGui::SmallButton("+ Add track"))
        {
            std::string error;
            if (!session.add_track(error))
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
                if (pending_removal.has_value() &&
                    !session.remove_track(*pending_removal, false, error))
                {
                    arrangement_error = error;
                }
                pending_removal.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Lane and files"))
            {
                if (pending_removal.has_value() &&
                    !session.remove_track(*pending_removal, true, error))
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
            draw->AddTriangleFilled(
                ImVec2(playhead_x - 5.0f, origin.y), ImVec2(playhead_x + 5.0f, origin.y),
                ImVec2(playhead_x, origin.y + 8.0f), to_imu32(rgba(220, 72, 60)));
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MouseWheel != 0.0f)
        {
            const float pointer = io.MousePos.x - canvas_x;
            if (io.KeyCtrl)
            {
                zoom_about(scale, pointer, io.MouseWheel > 0.0f ? K_ZOOM_STEP : 1.0 / K_ZOOM_STEP);
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
} // namespace ui
