/****************************************************************/
/* TrackTimeline.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/capture/TrackTimeline.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace SushiDSP
{
    TrackTimeline::TrackTimeline(TakeLibrary& library, const std::vector<Clip>& clips)
        : clips_(),
          count_(0),
          library_(library),
          retained_(),
          retained_count_(0)
    {
        if (clips.size() > kMaxClips)
        {
            throw std::runtime_error("a lane holds at most " + std::to_string(kMaxClips)
                                     + " clips, and " + std::to_string(clips.size())
                                     + " were given");
        }

        std::vector<Clip> sorted(clips);
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const Clip& left, const Clip& right)
                         { return left.start_frame < right.start_frame; });

        for (std::size_t index = 1; index < sorted.size(); ++index)
        {
            const Clip& previous = sorted[index - 1];
            const Clip& current = sorted[index];

            if (previous.start_frame + previous.length_frames > current.start_frame)
            {
                throw std::runtime_error("clips at start frames "
                                         + std::to_string(previous.start_frame) + " and "
                                         + std::to_string(current.start_frame) + " overlap");
            }
        }

        for (std::size_t index = 0; index < sorted.size(); ++index)
        {
            clips_[index] = sorted[index];
        }

        count_ = sorted.size();

        for (std::size_t index = 0; index < count_; ++index)
        {
            const Take* take = clips_[index].take;

            if (take == nullptr
                || std::find(retained_.begin(), retained_.begin() + retained_count_, take)
                    != retained_.begin() + retained_count_)
            {
                continue;
            }

            library_.retain(take);
            retained_[retained_count_] = take;
            ++retained_count_;
        }
    }

    TrackTimeline::~TrackTimeline()
    {
        for (std::size_t index = 0; index < retained_count_; ++index)
        {
            library_.release(retained_[index]);
        }
    }

    std::size_t TrackTimeline::clip_count() const noexcept
    {
        return count_;
    }

    const Clip& TrackTimeline::clip(std::size_t index) const noexcept
    {
        return clips_[index < count_ ? index : (count_ > 0 ? count_ - 1 : 0)];
    }

    std::size_t TrackTimeline::first_clip_at_or_after(std::uint64_t frame) const noexcept
    {
        for (std::size_t index = 0; index < count_; ++index)
        {
            if (clips_[index].start_frame + clips_[index].length_frames > frame)
            {
                return index;
            }
        }

        return count_;
    }

    float TrackTimeline::edge_gain(const Clip& clip, std::uint64_t clip_frame) noexcept
    {
        if (clip_frame >= clip.length_frames)
        {
            return 0.0f;
        }

        const std::uint64_t ramp = std::min(kFadeFrames, clip.length_frames / 2u);

        if (ramp == 0)
        {
            return 1.0f;
        }

        const std::uint64_t from_end = clip.length_frames - clip_frame;
        const std::uint64_t distance = std::min(clip_frame, from_end);

        if (distance >= ramp)
        {
            return 1.0f;
        }

        return static_cast<float>(distance) / static_cast<float>(ramp);
    }
}
