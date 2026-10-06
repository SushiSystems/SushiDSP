/**************************************************************************/
/* ArrangementLayout.hpp                                                  */
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
 * @file ArrangementLayout.hpp
 * @brief Declares the arrangement view's grid lines, clip spans and lane hit tests.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "SushiDSP/capture/MusicalClock.hpp"

#include "ui/TimelineScale.hpp"

namespace ui
{
    /// @brief Names one vertical line of the ruler's grid: the frame it stands on, the canvas x
    /// it is drawn at, and which bar and beat it opens.
    struct GridLine
    {
        std::uint64_t frame = 0;  ///< Holds the timeline frame the line stands on.
        float         x = 0.0f;   ///< Holds the canvas x in pixels the line is drawn at.
        std::uint64_t bar = 1;    ///< Holds the bar the line opens, counted from 1.
        std::uint32_t beat = 1;   ///< Holds the beat inside that bar, counted from 1.
        bool          is_bar = false; ///< Reports whether the line opens a bar rather than a beat.
    };

    /// @brief Names a clip's horizontal extent on a lane's canvas, in pixels.
    struct ClipSpan
    {
        float left = 0.0f;  ///< Holds the canvas x the clip's first sample is drawn at.
        float right = 0.0f; ///< Holds the canvas x one past the clip's last sample.
    };

    /// @brief Names where the lane canvases sat on screen the last time the view was drawn, so a
    /// drop that arrives outside the ImGui frame can be resolved to a lane and a frame.
    struct LaneGeometry
    {
        float       canvas_left = 0.0f;   ///< Holds the canvas's left edge in screen pixels.
        float       canvas_top = 0.0f;    ///< Holds the first lane's top in screen pixels.
        float       canvas_width = 0.0f;  ///< Holds the canvas width in pixels.
        float       lane_height = 0.0f;   ///< Holds one lane's height in pixels.
        float       lane_stride = 0.0f;   ///< Holds the distance between two lanes' tops.
        std::size_t lane_count = 0;       ///< Holds how many lanes were drawn.
    };

    /// @brief Names the lane a screen position falls on.
    /// @param geometry Where the lanes were drawn.
    /// @param screen_x The position's x in screen pixels.
    /// @param screen_y The position's y in screen pixels.
    /// @return The lane index, or std::nullopt for a position outside every lane's canvas,
    /// the gap between two lanes included.
    std::optional<std::size_t> lane_at(const LaneGeometry& geometry, float screen_x,
                                       float screen_y);

    /// @brief Converts a screen x to the canvas x route_file_drop reads a frame from.
    /// @param geometry Where the lanes were drawn.
    /// @param screen_x The position's x in screen pixels.
    /// @return The x relative to the canvas's left edge.
    float canvas_x_of(const LaneGeometry& geometry, float screen_x);

    /// @brief How many grid lines one canvas is ever asked for, so a zoom that fits an hour on
    /// screen costs a bounded walk instead of a beat per frame.
    inline constexpr std::size_t kMaxGridLines = 512;

    /// @brief How near two beat lines may fall before the beats between the bars are dropped,
    /// in pixels.
    inline constexpr float kMinimumBeatSpacing = 9.0f;

    /// @brief Lists the bar and beat lines a canvas of a width shows, left to right.
    /// @param scale The zoom and scroll the lines are placed through.
    /// @param clock The grid the bars and beats are read from.
    /// @param canvas_width The canvas width in pixels; a width at or below zero lists nothing.
    /// @return The lines, starting at the first grid frame at or after the scroll origin. Beat
    /// lines are left out when the zoom puts them closer together than kMinimumBeatSpacing.
    std::vector<GridLine> grid_lines(const TimelineScale& scale,
                                     const SushiDSP::MusicalClock& clock, float canvas_width);

    /// @brief Places a clip's rectangle on a lane's canvas.
    /// @param scale The zoom and scroll the clip is placed through.
    /// @param start_frame The timeline frame the clip's first sample lands on.
    /// @param length_frames How many frames the clip plays.
    /// @return The clip's left and right canvas x.
    ClipSpan clip_span(const TimelineScale& scale, std::uint64_t start_frame,
                       std::uint64_t length_frames);

    /// @brief Returns the frame a pointer at a canvas x drops a clip's first sample on.
    /// @param scale The zoom and scroll the x is read through.
    /// @param snap What the frame is rounded to.
    /// @param clock The grid the rounding uses.
    /// @param canvas_x The pointer's x in canvas pixels.
    /// @return The snapped frame; zero for an x left of the canvas origin.
    std::uint64_t dropped_frame(const TimelineScale& scale, SnapMode snap,
                                const SushiDSP::MusicalClock& clock, float canvas_x);

    /// @brief Returns the frame a clip dragged sideways by a pixel distance now starts at.
    /// @param scale The zoom the distance is read through.
    /// @param snap What the frame is rounded to.
    /// @param clock The grid the rounding uses.
    /// @param start_frame The frame the clip started the drag at.
    /// @param pixel_delta How far the pointer has moved, negative to the left.
    /// @return The snapped frame, clamped at zero so a clip never leaves the timeline's front.
    std::uint64_t dragged_start_frame(const TimelineScale& scale, SnapMode snap,
                                      const SushiDSP::MusicalClock& clock,
                                      std::uint64_t start_frame, float pixel_delta);
}
