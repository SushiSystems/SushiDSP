/**************************************************************************/
/* TrackTimeline.hpp                                                      */
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/TakeLibrary.hpp"

namespace SushiDSP
{
    /// @brief One run of a take placed on a lane: which take it reads, where on the timeline it
    /// lands, where in the take it starts and how much of it it plays.
    struct Clip
    {
        const Take*   take;          ///< Borrowed; owned by the TakeLibrary that built the timeline.
        std::uint64_t start_frame;   ///< Timeline frame the clip's first sample lands on.
        std::uint64_t take_offset;   ///< First take frame the clip plays.
        std::uint64_t length_frames; ///< How many take frames the clip plays.
    };

    static_assert(std::is_trivially_copyable<Clip>::value,
                  "A clip is read on the audio thread and must hold no refcount. Spec §5.");

    /// @brief Holds one lane's clips, sorted by start_frame and immutable once built. The control
    /// thread builds it and publishes it through an RtSwapSlot; the audio thread only reads it,
    /// and the take pointers a clip names stay valid because the constructor retained each of
    /// them in the library and the destructor, which a slot's reclaim() calls, is the only place
    /// they are released.
    /// @see docs/superpowers/specs/2026-09-08-arrangement-view-design.md §5
    class TrackTimeline
    {
    public:
        /// @brief Caps how many clips one lane holds, which is what keeps the lane a flat array
        /// the audio thread can walk.
        static constexpr std::size_t kMaxClips = 16;

        /// @brief How long a clip's de-click ramp lasts at each edge: 5 ms at 48 kHz.
        static constexpr std::uint64_t kFadeFrames = 240;

        /// @brief Builds a lane from clips in any order, and retains every distinct take once.
        /// @param library The owner of every take the clips name.
        /// @param clips The lane's clips; sorted by start_frame here, and must not overlap.
        /// @throw std::runtime_error when two clips overlap, naming both start frames, or when
        /// clips holds more than kMaxClips entries.
        /// @pre Called on the control thread.
        TrackTimeline(TakeLibrary& library, const std::vector<Clip>& clips);

        /// @brief Releases every take the constructor retained.
        /// @pre Called on the control thread, from a lane slot's reclaim().
        ~TrackTimeline();

        TrackTimeline(const TrackTimeline&) = delete;
        TrackTimeline& operator=(const TrackTimeline&) = delete;

        /// @brief Returns how many clips the lane holds.
        std::size_t clip_count() const noexcept;

        /// @brief Returns one clip, in start_frame order.
        /// @param index A clip index below clip_count(); a larger one returns the last clip.
        const Clip& clip(std::size_t index) const noexcept;

        /// @brief Returns the first clip whose end is past a frame. Audio thread; at most
        /// kMaxClips comparisons, no allocation, no lock and no throw.
        /// @param frame The timeline frame to search from.
        /// @return An index into clip(), or clip_count() when no clip reaches past the frame.
        std::size_t first_clip_at_or_after(std::uint64_t frame) const noexcept;

        /// @brief Returns the de-click ramp gain for a frame inside a clip. Audio thread.
        /// @param clip The clip the frame belongs to.
        /// @param clip_frame How far into the clip the frame lies, in frames.
        /// @return A gain in [0, 1]; zero at both edges and one across the middle.
        static float edge_gain(const Clip& clip, std::uint64_t clip_frame) noexcept;

    private:
        /// @brief Holds the lane's clips in start_frame order.
        std::array<Clip, kMaxClips> clips_;

        /// @brief Holds how many entries of clips_ are in use.
        std::size_t                 count_;

        /// @brief Refers to the library holding every take the clips name.
        TakeLibrary&                library_;

        /// @brief Holds each distinct take the constructor retained, so the destructor releases
        /// exactly the same set.
        std::array<const Take*, kMaxClips> retained_;

        /// @brief Holds how many entries of retained_ are in use.
        std::size_t                        retained_count_;
    };
}
