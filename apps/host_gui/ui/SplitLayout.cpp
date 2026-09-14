/**************************************************************************/
/* SplitLayout.cpp                                                       */
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

#include "ui/SplitLayout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ui
{
    namespace
    {
        Rect make_rect(float x, float y, float width, float height)
        {
            return Rect{ Vec2{ x, y }, Vec2{ x + width, y + height } };
        }

        Rect union_of(const Rect& a, const Rect& b)
        {
            return Rect{ Vec2{ std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y) },
                         Vec2{ std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y) } };
        }

        // One gap before each node and one after the last, so a caller can always drop at either
        // end; an empty stage collapses to the single gap `empty_width` wide.
        StageGeometry layout_stage(float start_x, float center_y, std::size_t count,
                                   const SplitMetrics& metrics, float empty_width)
        {
            StageGeometry stage;
            if (count == 0)
            {
                const Rect gap = make_rect(start_x, center_y - metrics.node_height / 2.0f,
                                           empty_width, metrics.node_height);
                stage.gaps.push_back(gap);
                stage.bounds = gap;
                return stage;
            }

            stage.nodes.reserve(count);
            stage.gaps.reserve(count + 1);
            float cursor = start_x;
            for (std::size_t i = 0; i < count; ++i)
            {
                const Rect gap = make_rect(cursor, center_y - metrics.node_height / 2.0f,
                                           metrics.wire_length, metrics.node_height);
                stage.gaps.push_back(gap);
                cursor += metrics.wire_length;

                const Rect node = make_rect(cursor, center_y - metrics.node_height / 2.0f,
                                            metrics.node_width, metrics.node_height);
                stage.nodes.push_back(node);
                cursor += metrics.node_width;
            }
            const Rect trailing_gap = make_rect(cursor, center_y - metrics.node_height / 2.0f,
                                                metrics.wire_length, metrics.node_height);
            stage.gaps.push_back(trailing_gap);
            cursor += metrics.wire_length;

            stage.bounds = make_rect(start_x, center_y - metrics.node_height / 2.0f,
                                     cursor - start_x, metrics.node_height);
            return stage;
        }

        float stage_width(const StageGeometry& stage)
        {
            return stage.bounds.max.x - stage.bounds.min.x;
        }

        Vec2 right_mid(const Rect& rect)
        {
            return Vec2{ rect.max.x, (rect.min.y + rect.max.y) / 2.0f };
        }

        Vec2 left_mid(const Rect& rect)
        {
            return Vec2{ rect.min.x, (rect.min.y + rect.max.y) / 2.0f };
        }

        // The routed cable the mockup's redrawSplitWires draws: an orthogonal step from the
        // source's right edge, halfway across the gap, into the target's left edge.
        std::vector<Vec2> route(Vec2 from, Vec2 to)
        {
            const float mid_x = from.x + (to.x - from.x) / 2.0f;
            return { from, Vec2{ mid_x, from.y }, Vec2{ mid_x, to.y }, to };
        }
    }

    SplitGeometry layout_split(std::size_t series_size, std::size_t pre_size,
                               const std::vector<std::size_t>& branch_sizes, std::size_t post_size,
                               const SplitMetrics& metrics)
    {
        SplitGeometry geometry;

        if (branch_sizes.empty())
        {
            const float center_y = std::max(metrics.io_size, metrics.node_height) / 2.0f;
            geometry.input = make_rect(0.0f, center_y - metrics.io_size / 2.0f, metrics.io_size,
                                       metrics.io_size);
            geometry.series = layout_stage(geometry.input.max.x, center_y, series_size, metrics,
                                           metrics.wire_length);
            const float output_x = geometry.series.bounds.max.x;
            geometry.output = make_rect(output_x, center_y - metrics.io_size / 2.0f,
                                        metrics.io_size, metrics.io_size);
            geometry.size = Vec2{ geometry.output.max.x, center_y * 2.0f };
            return geometry;
        }

        const std::size_t branch_count = branch_sizes.size();
        const float block_height = static_cast<float>(branch_count) * metrics.node_height
                                  + static_cast<float>(branch_count - 1) * metrics.row_gap;
        const float overall_height = std::max(block_height,
            std::max(metrics.node_height, std::max(metrics.io_size, metrics.glyph_size)));
        const float center_y = overall_height / 2.0f;
        const float block_top = center_y - block_height / 2.0f;

        geometry.input = make_rect(0.0f, center_y - metrics.io_size / 2.0f, metrics.io_size,
                                   metrics.io_size);

        geometry.pre = layout_stage(geometry.input.max.x, center_y, pre_size, metrics,
                                    metrics.wire_length);

        const Rect splitter = make_rect(geometry.pre.bounds.max.x, center_y - metrics.glyph_size / 2.0f,
                                        metrics.glyph_size, metrics.glyph_size);
        geometry.splitter = splitter;

        float widest_branch = 0.0f;
        std::vector<StageGeometry> rows;
        rows.reserve(branch_count);
        const float rows_start_x = splitter.max.x + metrics.branch_pad;
        for (std::size_t i = 0; i < branch_count; ++i)
        {
            const float row_center_y = block_top + metrics.node_height / 2.0f
                                      + static_cast<float>(i) * (metrics.node_height + metrics.row_gap);
            StageGeometry row = layout_stage(rows_start_x, row_center_y, branch_sizes[i], metrics,
                                             metrics.empty_slot_width);
            widest_branch = std::max(widest_branch, stage_width(row));
            rows.push_back(std::move(row));
        }
        // Every row shares the widest branch's width, so the mixer glyph lines up with one edge.
        for (StageGeometry& row : rows)
        {
            const float width = stage_width(row);
            if (width < widest_branch)
            {
                row.bounds.max.x = row.bounds.min.x + widest_branch;
            }
        }
        geometry.branches = std::move(rows);

        const Rect mixer = make_rect(rows_start_x + widest_branch + metrics.branch_pad,
                                     center_y - metrics.glyph_size / 2.0f, metrics.glyph_size,
                                     metrics.glyph_size);
        geometry.mixer = mixer;

        geometry.post = layout_stage(mixer.max.x, center_y, post_size, metrics, metrics.wire_length);

        geometry.output = make_rect(geometry.post.bounds.max.x, center_y - metrics.io_size / 2.0f,
                                    metrics.io_size, metrics.io_size);

        for (const StageGeometry& row : geometry.branches)
        {
            geometry.cables.push_back(route(right_mid(splitter), left_mid(row.bounds)));
            geometry.cables.push_back(route(right_mid(row.bounds), left_mid(mixer)));
        }

        geometry.size = Vec2{ geometry.output.max.x, overall_height };
        return geometry;
    }

    std::optional<std::size_t> nearest_gap(const StageGeometry& stage, Vec2 point)
    {
        if (stage.gaps.empty() || point.y < stage.bounds.min.y || point.y > stage.bounds.max.y)
        {
            return std::nullopt;
        }

        std::size_t best_index = 0;
        float best_distance = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < stage.gaps.size(); ++i)
        {
            const float center_x = (stage.gaps[i].min.x + stage.gaps[i].max.x) / 2.0f;
            const float distance = std::fabs(center_x - point.x);
            if (distance < best_distance)
            {
                best_distance = distance;
                best_index = i;
            }
        }
        return best_index;
    }
}
