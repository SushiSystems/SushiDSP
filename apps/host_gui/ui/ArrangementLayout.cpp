/**************************************************************************/
/* ArrangementLayout.cpp                                                  */
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


#include "ui/ArrangementLayout.hpp"

#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Returns the first frame of a beat named by its absolute index.
        /// @param index The beat, counted from zero at frame zero.
        /// @param clock The grid the beat belongs to.
        std::uint64_t frame_of_beat_index(std::uint64_t index, const SushiDSP::MusicalClock& clock)
        {
            const std::uint64_t beats_per_bar = clock.beats_per_bar();

            if (beats_per_bar == 0)
            {
                return 0;
            }

            return clock.frame_of(index / beats_per_bar + 1u,
                                  static_cast<std::uint32_t>(index % beats_per_bar) + 1u);
        }
    }

    std::vector<GridLine> grid_lines(const TimelineScale& scale,
                                     const SushiDSP::MusicalClock& clock, float canvas_width)
    {
        std::vector<GridLine> lines;
        const std::uint64_t   beats_per_bar = clock.beats_per_bar();

        if (canvas_width <= 0.0f || beats_per_bar == 0)
        {
            return lines;
        }

        const SushiDSP::BarBeat origin = clock.position_of(scale.scroll_frames());
        const std::uint64_t     first_index
            = (origin.bar - 1u) * beats_per_bar + (origin.beat - 1u);
        const std::uint64_t beat_frames = frame_of_beat_index(first_index + 1u, clock)
                                        - frame_of_beat_index(first_index, clock);
        const double beat_pixels
            = static_cast<double>(beat_frames) * scale.pixels_per_frame();
        const std::uint64_t step
            = beat_pixels < static_cast<double>(kMinimumBeatSpacing) ? beats_per_bar : 1u;
        const std::uint64_t start_index = first_index - first_index % step;

        for (std::uint64_t index = start_index; lines.size() < kMaxGridLines; index += step)
        {
            GridLine line;
            line.frame = frame_of_beat_index(index, clock);
            line.x = scale.x_of(line.frame);
            line.bar = index / beats_per_bar + 1u;
            line.beat = static_cast<std::uint32_t>(index % beats_per_bar) + 1u;
            line.is_bar = line.beat == 1u;

            if (line.x > canvas_width)
            {
                break;
            }

            if (line.x >= 0.0f)
            {
                lines.push_back(line);
            }
        }

        return lines;
    }

    ClipSpan clip_span(const TimelineScale& scale, std::uint64_t start_frame,
                       std::uint64_t length_frames)
    {
        ClipSpan span;
        span.left = scale.x_of(start_frame);
        span.right = scale.x_of(start_frame + length_frames);
        return span;
    }

    std::uint64_t dropped_frame(const TimelineScale& scale, SnapMode snap,
                                const SushiDSP::MusicalClock& clock, float canvas_x)
    {
        return scale.snap(scale.frame_of_x(canvas_x), snap, clock);
    }

    std::uint64_t dragged_start_frame(const TimelineScale& scale, SnapMode snap,
                                      const SushiDSP::MusicalClock& clock,
                                      std::uint64_t start_frame, float pixel_delta)
    {
        const double moved = static_cast<double>(start_frame)
                           + static_cast<double>(pixel_delta) / scale.pixels_per_frame();

        if (!(moved > 0.0))
        {
            return scale.snap(0u, snap, clock);
        }

        return scale.snap(static_cast<std::uint64_t>(std::llround(moved)), snap, clock);
    }
}
