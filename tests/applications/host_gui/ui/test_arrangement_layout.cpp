/**************************************************************************/
/* test_arrangement_layout.cpp                                            */
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
 * @file test_arrangement_layout.cpp
 * @brief Tests ArrangementLayout, which places grid lines and clip rectangles.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/musical_clock.hpp>

#include "ui/arrangement_layout.hpp"

namespace
{
    /// @brief Builds the grid every case measures against: 120 BPM in 4/4 at 48 kHz, where a
    /// beat is 24000 frames and a bar is 96000.
    /// @param clock The clock to set up.
    void make_four_four(SushiDSP::MusicalClock& clock)
    {
        clock.set_tempo(120.0);
        clock.set_meter(4u, 4u);
        clock.set_sample_rate(48000.0);
    }
} // namespace

TEST(Small_ArrangementLayout, ArrangementLayoutPlacesBarAndBeatLines)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.002);

    const std::vector<ui::GridLine> lines = ui::grid_lines(scale, clock, 800.0f);

    ASSERT_EQ(lines.size(), 17u);
    EXPECT_EQ(lines[0].frame, 0u);
    EXPECT_FLOAT_EQ(lines[0].x, 0.0f);
    EXPECT_TRUE(lines[0].is_bar);

    EXPECT_EQ(lines[1].frame, 24000u);
    EXPECT_FLOAT_EQ(lines[1].x, 48.0f);
    EXPECT_EQ(lines[1].beat, 2u);
    EXPECT_FALSE(lines[1].is_bar);

    EXPECT_EQ(lines[4].frame, 96000u);
    EXPECT_FLOAT_EQ(lines[4].x, 192.0f);
    EXPECT_EQ(lines[4].bar, 2u);
    EXPECT_TRUE(lines[4].is_bar);

    EXPECT_EQ(lines.back().frame, 384000u);
    EXPECT_FLOAT_EQ(lines.back().x, 768.0f);
    EXPECT_EQ(lines.back().bar, 5u);
}

TEST(Small_ArrangementLayout, ArrangementLayoutDropsTheBeatsWhenTheyCrowd)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.0002);

    const std::vector<ui::GridLine> lines = ui::grid_lines(scale, clock, 800.0f);

    ASSERT_FALSE(lines.empty());
    for (const ui::GridLine& line : lines)
    {
        EXPECT_TRUE(line.is_bar);
    }
    EXPECT_EQ(lines[1].bar, 2u);
    EXPECT_FLOAT_EQ(lines[1].x, 19.2f);
}

TEST(Small_ArrangementLayout, ArrangementLayoutStartsTheGridAtTheScrollOrigin)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.002);
    scale.set_scroll_frames(100000u);

    const std::vector<ui::GridLine> lines = ui::grid_lines(scale, clock, 400.0f);

    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines[0].frame, 120000u);
    EXPECT_FLOAT_EQ(lines[0].x, 40.0f);
    EXPECT_EQ(lines[0].bar, 2u);
    EXPECT_EQ(lines[0].beat, 2u);
}

TEST(Small_ArrangementLayout, ArrangementLayoutGivesAClipItsRectangle)
{
    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.002);

    const ui::ClipSpan span = ui::clip_span(scale, 96000u, 48000u);

    EXPECT_FLOAT_EQ(span.left, 192.0f);
    EXPECT_FLOAT_EQ(span.right, 288.0f);
}

TEST(Small_ArrangementLayout, ArrangementLayoutLandsADropOnTheSnappedBar)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.002);

    // Bar 9 is drawn at x 1536; a drop four pixels past it is the maintainer's acceptance case.
    EXPECT_EQ(ui::dropped_frame(scale, ui::SnapMode::Bar, clock, 1540.0f), clock.frame_of(9u, 1u));
    EXPECT_EQ(ui::dropped_frame(scale, ui::SnapMode::Bar, clock, 1540.0f), 768000u);
    EXPECT_EQ(ui::dropped_frame(scale, ui::SnapMode::Beat, clock, 1540.0f), 768000u);
    EXPECT_EQ(ui::dropped_frame(scale, ui::SnapMode::None, clock, 1540.0f), 770000u);
    EXPECT_EQ(ui::dropped_frame(scale, ui::SnapMode::Bar, clock, -80.0f), 0u);
}

TEST(Small_ArrangementLayout, ArrangementLayoutMovesADraggedClipByItsPixels)
{
    SushiDSP::MusicalClock clock;
    make_four_four(clock);

    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.002);

    EXPECT_EQ(ui::dragged_start_frame(scale, ui::SnapMode::Bar, clock, 96000u, 192.0f), 192000u);
    EXPECT_EQ(ui::dragged_start_frame(scale, ui::SnapMode::None, clock, 96000u, 100.0f), 146000u);
    EXPECT_EQ(ui::dragged_start_frame(scale, ui::SnapMode::Bar, clock, 96000u, 0.0f), 96000u);
    EXPECT_EQ(ui::dragged_start_frame(scale, ui::SnapMode::Bar, clock, 96000u, -1000.0f), 0u);
}

TEST(Small_ArrangementLayout, ArrangementLayoutNamesTheLaneAScreenPositionFallsOn)
{
    ui::LaneGeometry geometry;
    geometry.canvas_left = 200.0f;
    geometry.canvas_top = 100.0f;
    geometry.canvas_width = 800.0f;
    geometry.lane_height = 66.0f;
    geometry.lane_stride = 68.0f;
    geometry.lane_count = 8u;

    ASSERT_TRUE(ui::lane_at(geometry, 240.0f, 100.0f).has_value());
    EXPECT_EQ(*ui::lane_at(geometry, 240.0f, 100.0f), 0u);
    EXPECT_EQ(*ui::lane_at(geometry, 240.0f, 165.0f), 0u);
    EXPECT_EQ(*ui::lane_at(geometry, 240.0f, 168.0f), 1u);
    EXPECT_EQ(*ui::lane_at(geometry, 999.0f, 570.0f), 6u);
    EXPECT_EQ(*ui::lane_at(geometry, 200.0f, 576.0f), 7u);
}

TEST(Small_ArrangementLayout, ArrangementLayoutRefusesAPositionOutsideEveryLane)
{
    ui::LaneGeometry geometry;
    geometry.canvas_left = 200.0f;
    geometry.canvas_top = 100.0f;
    geometry.canvas_width = 800.0f;
    geometry.lane_height = 66.0f;
    geometry.lane_stride = 68.0f;
    geometry.lane_count = 8u;

    EXPECT_FALSE(ui::lane_at(geometry, 199.0f, 120.0f).has_value());
    EXPECT_FALSE(ui::lane_at(geometry, 1000.0f, 120.0f).has_value());
    EXPECT_FALSE(ui::lane_at(geometry, 240.0f, 99.0f).has_value());
    EXPECT_FALSE(ui::lane_at(geometry, 240.0f, 167.0f).has_value());
    EXPECT_FALSE(ui::lane_at(geometry, 240.0f, 644.0f).has_value());
    EXPECT_FALSE(ui::lane_at(ui::LaneGeometry{}, 0.0f, 0.0f).has_value());
}

TEST(Small_ArrangementLayout, ArrangementLayoutReadsADropXAgainstTheCanvasOrigin)
{
    ui::LaneGeometry geometry;
    geometry.canvas_left = 200.0f;

    EXPECT_FLOAT_EQ(ui::canvas_x_of(geometry, 200.0f), 0.0f);
    EXPECT_FLOAT_EQ(ui::canvas_x_of(geometry, 1540.0f + 200.0f), 1540.0f);
}
