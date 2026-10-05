/****************************************************************/
/* ArrangementView.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstddef>
#include <string>

#include <imgui.h>

#include "AudioImportQueue.hpp"
#include "RigController.hpp"

#include "ui/ArrangementLayout.hpp"
#include "ui/TimelineScale.hpp"

namespace ui
{
    /// @brief Draws the bar ruler, the playhead, eight lane headers and the clip canvas beside
    /// each of them, into the current window. Replaces DrawTrackPanel: one unit owns a track's
    /// whole appearance. Control thread only; it allocates and it draws.
    /// @param rig The rig whose player, clips, transport and musical clock the view drives.
    /// @param scale In/out: the zoom and the scroll every lane and the ruler share.
    /// @param snap In/out: what a dropped or dragged clip's frame is rounded to.
    /// @param imports The queue a dropped mp3 is sent to; the view also reports what it is
    /// decoding.
    /// @param arrangement_error In/out: the last refusal, shown until an action succeeds.
    /// @param geometry_out Receives where the lane canvases were drawn, so a drop arriving
    /// outside the ImGui frame can be resolved to a lane on the next one.
    /// @param header_width In/out: the lane-header column's width, resized by a divider drawn
    /// between the header column and the grid.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §6
    void DrawArrangementView(RigController& rig, TimelineScale& scale, SnapMode& snap,
                             SushiDSP::host::AudioImportQueue& imports,
                             std::string& arrangement_error, LaneGeometry& geometry_out,
                             float& header_width);

    /// @brief Places a file on a lane as a clip beginning at the frame a canvas position names.
    /// The one route both an ImGui payload drop and an Explorer drop take, so the two land a
    /// file on the same frame. Control thread only.
    /// @param rig The rig the clip is placed on.
    /// @param scale The zoom and scroll the position is read through.
    /// @param snap What the landing frame is rounded to.
    /// @param path The file to load. A file the tree cannot read is queued for decoding instead
    /// of loaded, and its clip appears on the frame the decode finishes.
    /// @param canvas_position The drop's position in the lane canvas's own pixels; only its x
    /// names a frame.
    /// @param track The lane index.
    /// @param imports The queue a file needing a decode is sent to.
    /// @param error_out Receives the failure message when the file will not load or the lane
    /// refuses the clip.
    /// @return True when the lane holds the clip or the file is queued for decoding.
    bool route_file_drop(RigController& rig, const TimelineScale& scale, SnapMode snap,
                         const std::string& path, ImVec2 canvas_position, std::size_t track,
                         SushiDSP::host::AudioImportQueue& imports, std::string& error_out);
}
