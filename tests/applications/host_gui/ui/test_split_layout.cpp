/**************************************************************************/
/* test_split_layout.cpp                                                  */
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
 * @file test_split_layout.cpp
 * @brief Tests the node and cable placement of SplitLayout.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "ui/split_layout.hpp"

TEST(SplitLayout, SplitLayoutSeriesPlacesNodesLeftToRightWithOneGapMore)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(2, 0, {}, 0, metrics);

    ASSERT_EQ(geometry.series.nodes.size(), 2u);
    ASSERT_EQ(geometry.series.gaps.size(), 3u);
    EXPECT_FLOAT_EQ(geometry.input.min.x, 0.0f);
    EXPECT_FLOAT_EQ(geometry.input.max.x, 34.0f);
    EXPECT_FLOAT_EQ(geometry.series.gaps[0].min.x, 34.0f);
    EXPECT_FLOAT_EQ(geometry.series.nodes[0].min.x, 64.0f);
    EXPECT_FLOAT_EQ(geometry.series.nodes[0].max.x, 142.0f);
    EXPECT_FLOAT_EQ(geometry.series.gaps[1].min.x, 142.0f);
    EXPECT_FLOAT_EQ(geometry.series.nodes[1].min.x, 172.0f);
    EXPECT_FLOAT_EQ(geometry.series.nodes[1].max.x, 250.0f);
    EXPECT_FLOAT_EQ(geometry.series.gaps[2].min.x, 250.0f);
    EXPECT_FLOAT_EQ(geometry.series.gaps[2].max.x, 280.0f);
    EXPECT_FLOAT_EQ(geometry.output.min.x, 280.0f);
    EXPECT_FLOAT_EQ(geometry.output.max.x, 314.0f);
    EXPECT_FALSE(geometry.splitter.has_value());
    EXPECT_FALSE(geometry.mixer.has_value());
    EXPECT_FLOAT_EQ(geometry.size.x, 314.0f);
    EXPECT_FLOAT_EQ(geometry.size.y, 86.0f);
}

TEST(SplitLayout, SplitLayoutThreeWayStacksRowsAroundTheMainLine)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(0, 0, {1, 1, 1}, 0, metrics);

    ASSERT_EQ(geometry.branches.size(), 3u);
    ASSERT_TRUE(geometry.splitter.has_value());
    ASSERT_TRUE(geometry.mixer.has_value());

    // block_height = 3*86 + 2*14 = 286; centre = 143, which is the middle row's centre.
    const float center_y = 143.0f;
    EXPECT_FLOAT_EQ((geometry.splitter->min.y + geometry.splitter->max.y) / 2.0f, center_y);
    EXPECT_FLOAT_EQ((geometry.mixer->min.y + geometry.mixer->max.y) / 2.0f, center_y);

    const float row0_center =
        (geometry.branches[0].nodes[0].min.y + geometry.branches[0].nodes[0].max.y) / 2.0f;
    const float row1_center =
        (geometry.branches[1].nodes[0].min.y + geometry.branches[1].nodes[0].max.y) / 2.0f;
    const float row2_center =
        (geometry.branches[2].nodes[0].min.y + geometry.branches[2].nodes[0].max.y) / 2.0f;
    EXPECT_FLOAT_EQ(row0_center, 43.0f);
    EXPECT_FLOAT_EQ(row1_center, center_y);
    EXPECT_FLOAT_EQ(row2_center, 243.0f);
    // Adjacent rows are exactly row_gap apart.
    EXPECT_FLOAT_EQ(geometry.branches[1].bounds.min.y - geometry.branches[0].bounds.max.y,
                    metrics.row_gap);
    EXPECT_FLOAT_EQ(geometry.branches[2].bounds.min.y - geometry.branches[1].bounds.max.y,
                    metrics.row_gap);
}

TEST(SplitLayout, SplitLayoutCablesStartAtSplitterAndEndAtMixer)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(0, 0, {1, 1}, 0, metrics);

    ASSERT_TRUE(geometry.splitter.has_value());
    ASSERT_TRUE(geometry.mixer.has_value());
    ASSERT_EQ(geometry.cables.size(), 4u); // two polylines (in, out) per branch

    const float splitter_right_x = geometry.splitter->max.x;
    const float splitter_mid_y = (geometry.splitter->min.y + geometry.splitter->max.y) / 2.0f;
    const float mixer_left_x = geometry.mixer->min.x;
    const float mixer_mid_y = (geometry.mixer->min.y + geometry.mixer->max.y) / 2.0f;

    // Branch 0's "in" cable starts at the splitter's right edge.
    ASSERT_FALSE(geometry.cables[0].empty());
    EXPECT_FLOAT_EQ(geometry.cables[0].front().x, splitter_right_x);
    EXPECT_FLOAT_EQ(geometry.cables[0].front().y, splitter_mid_y);

    // Branch 0's "out" cable ends at the mixer's left edge.
    ASSERT_FALSE(geometry.cables[1].empty());
    EXPECT_FLOAT_EQ(geometry.cables[1].back().x, mixer_left_x);
    EXPECT_FLOAT_EQ(geometry.cables[1].back().y, mixer_mid_y);

    // Branch 1's "in" cable also starts at the splitter, branch 1's "out" also ends at the mixer.
    EXPECT_FLOAT_EQ(geometry.cables[2].front().x, splitter_right_x);
    EXPECT_FLOAT_EQ(geometry.cables[3].back().x, mixer_left_x);
}

TEST(SplitLayout, SplitLayoutEmptyBranchStillOffersOneGap)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(0, 0, {0}, 0, metrics);

    ASSERT_EQ(geometry.branches.size(), 1u);
    EXPECT_TRUE(geometry.branches[0].nodes.empty());
    ASSERT_EQ(geometry.branches[0].gaps.size(), 1u);
    EXPECT_FLOAT_EQ(geometry.branches[0].gaps[0].max.x - geometry.branches[0].gaps[0].min.x,
                    metrics.empty_slot_width);
}

TEST(SplitLayout, SplitLayoutNearestGapPicksTheClosestCentre)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(2, 0, {}, 0, metrics);

    // gap centres: 49, 157, 265; 170 sits closest to gap 1's centre (157).
    const auto index = ui::nearest_gap(geometry.series, ui::Vec2{170.0f, 43.0f});
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(*index, 1u);
}

TEST(SplitLayout, SplitLayoutNearestGapRejectsAPointOutsideTheStage)
{
    const ui::SplitMetrics metrics;
    const ui::SplitGeometry geometry = ui::layout_split(2, 0, {}, 0, metrics);

    const auto index = ui::nearest_gap(geometry.series, ui::Vec2{170.0f, 200.0f});
    EXPECT_FALSE(index.has_value());
}
