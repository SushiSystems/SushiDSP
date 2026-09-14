/**************************************************************************/
/* SplitLayout.hpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "ui/Geometry.hpp"

namespace ui
{
    /// @brief Every measurement the split-chain view needs, in panel-local units.
    struct SplitMetrics
    {
        float node_width = 78.0f;
        float node_height = 86.0f;
        float wire_length = 30.0f;
        float io_size = 34.0f;
        float glyph_size = 34.0f;
        float row_gap = 14.0f;
        float branch_pad = 46.0f;
        float empty_slot_width = 78.0f;
        float corner_radius = 6.0f;
    };

    /// @brief An axis-aligned box in panel-local units.
    struct Rect
    {
        Vec2 min;
        Vec2 max;
    };

    /// @brief One stage's nodes and the drop gaps between and around them.
    struct StageGeometry
    {
        std::vector<Rect> nodes;
        std::vector<Rect> gaps;
        Rect              bounds;
    };

    /// @brief Every rectangle and cable the chain view draws for one chain type.
    struct SplitGeometry
    {
        Rect                            input;
        Rect                            output;
        std::optional<Rect>             splitter;
        std::optional<Rect>             mixer;
        StageGeometry                   series;
        StageGeometry                   pre;
        StageGeometry                   post;
        std::vector<StageGeometry>      branches;
        std::vector<std::vector<Vec2>>  cables;
        Vec2                            size;
    };

    /// @brief Places IN/OUT, every stage's nodes and drop gaps, and the splitter/mixer cables.
    /// Series (`branch_sizes` empty) lays out `series_size` nodes between IN and OUT and leaves
    /// `pre`, `post`, `branches`, `splitter`, `mixer` and `cables` empty. Otherwise it lays out
    /// `pre_size` nodes, a splitter glyph, one row per entry of `branch_sizes` stacked around the
    /// block's vertical centre, a mixer glyph, then `post_size` nodes; an empty branch still gets
    /// one `empty_slot_width` drop gap so it stays a valid drop target.
    /// @param series_size Node count for the Series stage; ignored unless `branch_sizes` is empty.
    /// @param pre_size Node count for the stage before the splitter glyph.
    /// @param branch_sizes One entry per branch, its node count; empty selects Series.
    /// @param post_size Node count for the stage after the mixer glyph.
    /// @param metrics The measurements to place everything with.
    /// @return The full geometry, `size` holding its overall bounding box.
    SplitGeometry layout_split(std::size_t series_size, std::size_t pre_size,
                               const std::vector<std::size_t>& branch_sizes, std::size_t post_size,
                               const SplitMetrics& metrics);

    /// @brief Finds the drop gap in `stage` whose centre lies nearest `point.x`.
    /// @param stage The stage to search; its `gaps` must be non-empty.
    /// @param point The pointer position to test, in the same coordinates as `stage.bounds`.
    /// @return The gap's index, or `std::nullopt` when `point.y` falls outside `stage.bounds`.
    std::optional<std::size_t> nearest_gap(const StageGeometry& stage, Vec2 point);
}
