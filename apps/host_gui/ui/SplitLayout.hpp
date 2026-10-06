/**************************************************************************/
/* SplitLayout.hpp                                                        */
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
 * @file SplitLayout.hpp
 * @brief Declares the chain view's geometry for every chain type.
 * @author Mustafa Garip
 */

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
    /// @param series_size Node count for the Series stage; ignored unless `branch_sizes` is empty.
    /// @param pre_size Node count for the stage before the splitter glyph.
    /// @param branch_sizes One entry per branch, its node count; empty selects Series.
    /// @param post_size Node count for the stage after the mixer glyph.
    /// @param metrics The measurements to place everything with.
    /// @return The full geometry, `size` holding its overall bounding box.
    /// @see apps/host_gui/ui/README.md
    SplitGeometry layout_split(std::size_t series_size, std::size_t pre_size,
                               const std::vector<std::size_t>& branch_sizes, std::size_t post_size,
                               const SplitMetrics& metrics);

    /// @brief Finds the drop gap in `stage` whose centre lies nearest `point.x`.
    /// @param stage The stage to search; its `gaps` must be non-empty.
    /// @param point The pointer position to test, in the same coordinates as `stage.bounds`.
    /// @return The gap's index, or `std::nullopt` when `point.y` falls outside `stage.bounds`.
    std::optional<std::size_t> nearest_gap(const StageGeometry& stage, Vec2 point);
}
