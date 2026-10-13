/**************************************************************************/
/* arrangement_view.hpp                                                   */
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
 * @file arrangement_view.hpp
 * @brief Declares the arrangement view and the route a dropped file takes onto a lane.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <string>

#include <imgui.h>

#include "audio_import_queue.hpp"
#include "rig_controller.hpp"
#include "session.hpp"

#include "ui/arrangement_layout.hpp"
#include "ui/timeline_scale.hpp"

namespace ui
{
    /// @brief Draws the bar ruler, the playhead, the track headers and each track's clip canvas.
    /// A header selects its track, bypasses its rig, opens its rig and track files, and holds
    /// its strip.
    /// @param session The session whose tracks are drawn and edited.
    /// @param scale In/out: the zoom and the scroll every lane and the ruler share.
    /// @param snap In/out: what a dropped or dragged clip's frame is rounded to.
    /// @param arrangement_error In/out: the last refusal, shown until an action succeeds.
    /// @param geometry_out Receives where the lane canvases were drawn.
    /// @param header_width In/out: the lane-header column's width.
    /// @pre Called on the control thread.
    /// @see applications/host_gui/ui/README.md
    void DrawArrangementView(Session& session, TimelineScale& scale, SnapMode& snap,
                             SushiDSP::host::AudioImportQueue& imports,
                             std::string& arrangement_error, LaneGeometry& geometry_out,
                             float& header_width);

    /// @brief Places a file on a lane as a clip beginning at the frame a canvas position names.
    /// @param path The file to load; one the tree cannot read is queued for decoding instead.
    /// @param canvas_position The drop's position in the lane canvas's pixels; only x is read.
    /// @param track The lane index.
    /// @param error_out Receives the message when the file or the lane refuses.
    /// @return True when the lane holds the clip or the file is queued for decoding.
    /// @pre Called on the control thread.
    /// @see applications/host_gui/ui/README.md
    bool route_file_drop(RigController& rig, const TimelineScale& scale, SnapMode snap,
                         const std::string& path, ImVec2 canvas_position, std::size_t track,
                         SushiDSP::host::AudioImportQueue& imports, std::string& error_out);
} // namespace ui
