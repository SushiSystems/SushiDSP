/****************************************************************/
/* ArrangementView.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file ArrangementView.hpp
 * @brief Declares the arrangement view and the route a dropped file takes onto a lane.
 * @author Mustafa Garip
 */

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
    /// @brief Draws the bar ruler, the playhead, the lane headers and each lane's clip canvas.
    /// @param scale In/out: the zoom and the scroll every lane and the ruler share.
    /// @param snap In/out: what a dropped or dragged clip's frame is rounded to.
    /// @param arrangement_error In/out: the last refusal, shown until an action succeeds.
    /// @param geometry_out Receives where the lane canvases were drawn.
    /// @param header_width In/out: the lane-header column's width.
    /// @pre Called on the control thread.
    /// @see apps/host_gui/ui/README.md
    void DrawArrangementView(RigController& rig, TimelineScale& scale, SnapMode& snap,
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
    /// @see apps/host_gui/ui/README.md
    bool route_file_drop(RigController& rig, const TimelineScale& scale, SnapMode snap,
                         const std::string& path, ImVec2 canvas_position, std::size_t track,
                         SushiDSP::host::AudioImportQueue& imports, std::string& error_out);
}
