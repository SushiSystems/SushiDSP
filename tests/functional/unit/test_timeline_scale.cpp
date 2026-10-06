/**************************************************************************/
/* test_timeline_scale.cpp                                                */
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
 * @file test_timeline_scale.cpp
 * @brief Tests the frame-to-pixel mapping and snapping of TimelineScale.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/capture/MusicalClock.hpp"

#include "ui/TimelineScale.hpp"

TEST(Unit_Dsp, TimelineScaleRoundTripsAFrameThroughAPixel)
{
    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.01);
    scale.set_scroll_frames(48000u);

    EXPECT_FLOAT_EQ(scale.x_of(50000u), 20.0f);
    EXPECT_EQ(scale.frame_of_x(20.0f), 50000u);
    EXPECT_EQ(scale.frame_of_x(scale.x_of(123400u)), 123400u);
}

TEST(Unit_Dsp, TimelineScalePutsTheScrollOriginAtXZero)
{
    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.01);
    scale.set_scroll_frames(96000u);

    EXPECT_FLOAT_EQ(scale.x_of(96000u), 0.0f);
    EXPECT_EQ(scale.frame_of_x(0.0f), 96000u);
    EXPECT_EQ(scale.scroll_frames(), 96000u);
}

TEST(Unit_Dsp, TimelineScaleSnapsToTheNearestBar)
{
    SushiDSP::MusicalClock clock;   // 120 BPM, 4/4, 48 kHz: a bar is 96000 frames
    ui::TimelineScale      scale;

    EXPECT_EQ(clock.frame_of(2u, 1u), 96000u);
    EXPECT_EQ(clock.frame_of(3u, 1u), 192000u);

    EXPECT_EQ(scale.snap(100000u, ui::SnapMode::Bar, clock), 96000u);
    EXPECT_EQ(scale.snap(110000u, ui::SnapMode::Bar, clock), 96000u);
    EXPECT_EQ(scale.snap(150000u, ui::SnapMode::Bar, clock), 192000u);
    EXPECT_EQ(scale.snap(96000u, ui::SnapMode::Bar, clock), 96000u);
    EXPECT_EQ(scale.snap(0u, ui::SnapMode::Bar, clock), 0u);
}

TEST(Unit_Dsp, TimelineScaleSnapsToTheNearestBeat)
{
    SushiDSP::MusicalClock clock;   // 120 BPM, 4/4, 48 kHz: a beat is 24000 frames
    ui::TimelineScale      scale;

    EXPECT_EQ(scale.snap(100000u, ui::SnapMode::Beat, clock), 96000u);
    EXPECT_EQ(scale.snap(110000u, ui::SnapMode::Beat, clock), 120000u);
    EXPECT_EQ(scale.snap(24000u, ui::SnapMode::Beat, clock), 24000u);
    EXPECT_EQ(scale.snap(12000u, ui::SnapMode::Beat, clock), 0u);   // a tie takes the earlier
}

TEST(Unit_Dsp, TimelineScaleSnapModeNoneReturnsTheFrameUnchanged)
{
    SushiDSP::MusicalClock clock;
    ui::TimelineScale      scale;

    EXPECT_EQ(scale.snap(100000u, ui::SnapMode::None, clock), 100000u);
    EXPECT_EQ(scale.snap(1u, ui::SnapMode::None, clock), 1u);
}

TEST(Unit_Dsp, TimelineScaleDoesNotUnderflowBeforeFrameZero)
{
    ui::TimelineScale scale;
    scale.set_pixels_per_frame(0.01);
    scale.set_scroll_frames(0u);

    EXPECT_EQ(scale.frame_of_x(-1.0f), 0u);
    EXPECT_EQ(scale.frame_of_x(-100000.0f), 0u);

    scale.set_scroll_frames(1000u);
    EXPECT_EQ(scale.frame_of_x(-1000.0f), 0u);   // 1000 frames of scroll, 100000 frames left
}
