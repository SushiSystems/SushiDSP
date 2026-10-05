/****************************************************************/
/* TimelineScale.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file TimelineScale.cpp
 * @brief Implements the conversion between timeline frames and canvas pixels.
 * @author Mustafa Garip
 */

#include "ui/TimelineScale.hpp"

#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Returns the first frame of a beat named by its absolute index.
        /// @param index The beat, counted from zero at frame zero.
        /// @param clock The grid the beat belongs to.
        std::uint64_t frame_of_beat(std::uint64_t index, const SushiDSP::MusicalClock& clock)
        {
            const std::uint64_t beats_per_bar = clock.beats_per_bar();

            if (beats_per_bar == 0)
            {
                return 0;
            }

            return clock.frame_of(index / beats_per_bar + 1u,
                                  static_cast<std::uint32_t>(index % beats_per_bar) + 1u);
        }

        /// @brief Returns whichever of two grid frames a frame lies nearer, taking the earlier
        /// one when it lies exactly between them.
        /// @param frame The frame to round.
        /// @param lower The grid frame at or before it.
        /// @param upper The next grid frame.
        std::uint64_t nearer(std::uint64_t frame, std::uint64_t lower, std::uint64_t upper)
        {
            return frame - lower <= upper - frame ? lower : upper;
        }
    }

    void TimelineScale::set_pixels_per_frame(double pixels_per_frame)
    {
        if (!std::isfinite(pixels_per_frame) || pixels_per_frame <= 0.0)
        {
            return;
        }

        pixels_per_frame_ = pixels_per_frame;
    }

    double TimelineScale::pixels_per_frame() const
    {
        return pixels_per_frame_;
    }

    void TimelineScale::set_scroll_frames(std::uint64_t frames)
    {
        scroll_frames_ = frames;
    }

    std::uint64_t TimelineScale::scroll_frames() const
    {
        return scroll_frames_;
    }

    float TimelineScale::x_of(std::uint64_t frame) const
    {
        const double offset = static_cast<double>(frame) - static_cast<double>(scroll_frames_);
        return static_cast<float>(offset * pixels_per_frame_);
    }

    std::uint64_t TimelineScale::frame_of_x(float x) const
    {
        const double frames
            = static_cast<double>(x) / pixels_per_frame_ + static_cast<double>(scroll_frames_);

        if (!(frames > 0.0))
        {
            return 0;
        }

        return static_cast<std::uint64_t>(std::llround(frames));
    }

    std::uint64_t TimelineScale::snap(std::uint64_t frame, SnapMode mode,
                                      const SushiDSP::MusicalClock& clock) const
    {
        if (mode == SnapMode::None)
        {
            return frame;
        }

        const SushiDSP::BarBeat placed = clock.position_of(frame);
        const std::uint64_t     beats_per_bar = clock.beats_per_bar();

        if (beats_per_bar == 0)
        {
            return frame;
        }

        const std::uint64_t beat_index = (placed.bar - 1u) * beats_per_bar + (placed.beat - 1u);
        const std::uint64_t step = mode == SnapMode::Bar ? beats_per_bar : 1u;
        const std::uint64_t lower_index = beat_index - beat_index % step;
        const std::uint64_t lower = frame_of_beat(lower_index, clock);
        const std::uint64_t upper = frame_of_beat(lower_index + step, clock);

        // position_of floors; apps/host_gui/ui/README.md says what that does to a grid frame.
        if (frame < lower)
        {
            return lower;
        }

        return nearer(frame, lower, upper);
    }
}
