/****************************************************************/
/* TimelineScale.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstdint>

#include "SushiDSP/capture/MusicalClock.hpp"

namespace ui
{
    /// @brief What a frame is rounded to when it is dropped or dragged onto a lane.
    enum class SnapMode : std::uint32_t
    {
        None = 0u,
        Beat = 1u,
        Bar = 2u
    };

    /// @brief Converts between timeline frames and canvas pixels, and rounds a frame to the
    /// musical grid. The ruler, a clip's rectangle and a dropped file's landing frame all go
    /// through this one object, so a clip is never drawn a pixel off the bar it plays on.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §6
    class TimelineScale
    {
    public:
        /// @brief Sets the zoom, in pixels a frame covers.
        /// @param pixels_per_frame Pixels per frame; a value that is not finite and greater
        /// than zero leaves the zoom as it was.
        void set_pixels_per_frame(double pixels_per_frame);

        /// @brief Returns the zoom, in pixels a frame covers.
        double pixels_per_frame() const;

        /// @brief Sets the frame the canvas draws at x zero.
        /// @param frames The leftmost visible frame.
        void set_scroll_frames(std::uint64_t frames);

        /// @brief Returns the frame the canvas draws at x zero.
        std::uint64_t scroll_frames() const;

        /// @brief Returns the canvas x a frame is drawn at.
        /// @param frame The timeline frame to place.
        /// @return The x in pixels, negative for a frame left of the scroll origin.
        float x_of(std::uint64_t frame) const;

        /// @brief Returns the frame a canvas x lands on, clamped at frame zero so a pointer
        /// left of the origin never wraps into a huge frame.
        /// @param x The canvas x in pixels.
        std::uint64_t frame_of_x(float x) const;

        /// @brief Rounds a frame to the nearest bar, the nearest beat, or leaves it alone.
        /// @param frame The frame to round.
        /// @param mode What to round to.
        /// @param clock The grid the bars and beats come from.
        /// @return The rounded frame; the frame itself when mode is None.
        std::uint64_t snap(std::uint64_t frame, SnapMode mode,
                           const SushiDSP::MusicalClock& clock) const;

    private:
        /// @brief Holds the zoom, in pixels a frame covers.
        double        pixels_per_frame_{ 0.002 };

        /// @brief Holds the frame the canvas draws at x zero.
        std::uint64_t scroll_frames_{ 0 };
    };
}
