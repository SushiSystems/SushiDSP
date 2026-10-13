/**************************************************************************/
/* track_player.cpp                                                       */
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
 * @file track_player.cpp
 * @brief Implements the mixing of clip timelines at the transport's position and speed.
 * @author Mustafa Garip
 */

#include "SushiDSP/capture/track_player.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "SushiDSP/core/denormal_guard.hpp"

namespace SushiDSP
{
    namespace
    {
        constexpr float K_QUARTER_PI = 0.78539816339744830961f;

        float clamp_pan(float pan)
        {
            return std::min(1.0f, std::max(-1.0f, pan));
        }
    } // namespace

    void TrackPlayer::prepare(std::uint32_t max_block_frames)
    {
        prepared_block_frames_ = std::min(max_block_frames, K_MAX_BLOCK_FRAMES);

        for (Track& track : tracks_)
        {
            track.reader.prepare(VariRateReader::K_MAX_CHANNELS, prepared_block_frames_);
            track.reader_primed = false;
        }
    }

    TakeLibrary& TrackPlayer::takes() noexcept
    {
        return takes_;
    }

    void TrackPlayer::set_timeline(std::size_t track, std::unique_ptr<TrackTimeline> timeline)
    {
        if (track >= K_MAX_TRACKS || timeline == nullptr)
        {
            return;
        }
        tracks_[track].timeline.publish(std::move(timeline));
    }

    void TrackPlayer::set_take(std::size_t index, std::unique_ptr<Take> take)
    {
        if (index >= K_MAX_TRACKS || take == nullptr)
        {
            return;
        }

        const std::uint64_t frames = static_cast<std::uint64_t>(take->frames());
        const Take* const borrowed = takes_.insert(std::unique_ptr<const Take>(std::move(take)));

        std::vector<Clip> clips;
        clips.push_back(Clip{borrowed, 0u, 0u, frames});
        set_timeline(index, std::unique_ptr<TrackTimeline>(new TrackTimeline(takes_, clips)));
    }

    void TrackPlayer::set_gain(std::size_t index, float gain)
    {
        if (index < K_MAX_TRACKS)
        {
            tracks_[index].gain.store(gain, std::memory_order_release);
        }
    }

    void TrackPlayer::set_pan(std::size_t index, float pan)
    {
        if (index < K_MAX_TRACKS)
        {
            tracks_[index].pan.store(clamp_pan(pan), std::memory_order_release);
        }
    }

    void TrackPlayer::set_mute(std::size_t index, bool muted)
    {
        if (index < K_MAX_TRACKS)
        {
            tracks_[index].muted.store(muted, std::memory_order_release);
        }
    }

    void TrackPlayer::set_solo(std::size_t index, bool soloed)
    {
        if (index < K_MAX_TRACKS)
        {
            tracks_[index].soloed.store(soloed, std::memory_order_release);
        }
    }

    void TrackPlayer::reclaim()
    {
        // Lane slots are reclaimed before the library; see modules/nodes/capture/README.md.
        for (Track& track : tracks_)
        {
            track.timeline.reclaim();
        }

        takes_.collect();
    }

    float TrackPlayer::gain(std::size_t index) const
    {
        return index < K_MAX_TRACKS ? tracks_[index].gain.load(std::memory_order_acquire) : 0.0f;
    }

    float TrackPlayer::pan(std::size_t index) const
    {
        return index < K_MAX_TRACKS ? tracks_[index].pan.load(std::memory_order_acquire) : 0.0f;
    }

    bool TrackPlayer::muted(std::size_t index) const
    {
        return index < K_MAX_TRACKS && tracks_[index].muted.load(std::memory_order_acquire);
    }

    bool TrackPlayer::soloed(std::size_t index) const
    {
        return index < K_MAX_TRACKS && tracks_[index].soloed.load(std::memory_order_acquire);
    }

    float TrackPlayer::clip_gain(const Clip& clip, std::uint64_t clip_frame) noexcept
    {
        const std::uint64_t take_frames =
            clip.take != nullptr ? static_cast<std::uint64_t>(clip.take->frames()) : 0u;
        const bool head_is_cut = clip.take_offset > 0u;
        const bool tail_is_cut = clip.take_offset + clip.length_frames < take_frames;

        // The ramp is symmetric about the clip's middle; see modules/nodes/capture/README.md.
        const std::uint64_t middle = clip.length_frames / 2u;
        std::uint64_t query = middle;

        if (head_is_cut && tail_is_cut)
        {
            query = clip_frame;
        }
        else if (head_is_cut)
        {
            query = std::min(clip_frame, middle);
        }
        else if (tail_is_cut)
        {
            query = std::max(clip_frame, middle);
        }

        return TrackTimeline::edge_gain(clip, query);
    }

    std::uint32_t TrackPlayer::frames_to_reach(std::uint64_t distance, std::uint64_t speed_q32,
                                               std::uint32_t remaining) noexcept
    {
        if (distance == 0u || speed_q32 == 0u)
        {
            return 0u;
        }

        if (distance >= (1ull << 31))
        {
            return remaining;
        }

        const std::uint64_t needed = ((distance << 32) + speed_q32 - 1u) / speed_q32;
        return needed >= static_cast<std::uint64_t>(remaining) ? remaining
                                                               : static_cast<std::uint32_t>(needed);
    }

    std::uint64_t TrackPlayer::distance_covered(std::uint32_t output_frames,
                                                std::uint64_t speed_q32) noexcept
    {
        return (static_cast<std::uint64_t>(output_frames) * speed_q32) >> 32;
    }

    void TrackPlayer::mix_run(const float* left_source, const float* right_source,
                              float* const* outputs, std::uint32_t channels, std::uint32_t offset,
                              std::uint32_t count, const Clip& clip, std::uint64_t clip_frame,
                              MixGains gains) noexcept
    {
        if (count == 0u || left_source == nullptr || right_source == nullptr)
        {
            return;
        }

        // The gain is smallest at one end of a run; see modules/nodes/capture/README.md.
        const bool flat =
            clip_gain(clip, clip_frame) == 1.0f && clip_gain(clip, clip_frame + count - 1u) == 1.0f;

        float* const out_left = outputs[0];

        if (channels < 2)
        {
            // A mono output carries no pan, so only the clip's own two channels fold together.
            for (std::uint32_t frame = 0; frame < count; ++frame)
            {
                const float ramp = flat ? 1.0f : clip_gain(clip, clip_frame + frame);
                const float source = 0.5f * (left_source[frame] + right_source[frame]);
                out_left[offset + frame] += gains.mono * source * ramp;
            }
            return;
        }

        float* const out_right = outputs[1];

        for (std::uint32_t frame = 0; frame < count; ++frame)
        {
            const float ramp = flat ? 1.0f : clip_gain(clip, clip_frame + frame);
            out_left[offset + frame] += left_source[frame] * gains.left * ramp;
            out_right[offset + frame] += right_source[frame] * gains.right * ramp;
        }
    }

    void TrackPlayer::mix_clip_at_unity(const Clip& clip, float* const* outputs,
                                        std::uint32_t channels, std::uint32_t offset,
                                        std::uint32_t count, std::uint64_t clip_frame,
                                        MixGains gains) noexcept
    {
        const Take* const take = clip.take;

        if (take == nullptr || take->channels() == 0u)
        {
            return;
        }

        const std::uint64_t take_frames = static_cast<std::uint64_t>(take->frames());
        const std::uint64_t take_position = clip.take_offset + clip_frame;

        if (take_position >= take_frames)
        {
            return;
        }

        const std::uint32_t readable =
            static_cast<std::uint32_t>(std::min<std::uint64_t>(count, take_frames - take_position));
        const float* const left_source = take->channel(0);
        const float* const right_source = take->channels() >= 2u ? take->channel(1) : left_source;

        if (left_source == nullptr || right_source == nullptr)
        {
            return;
        }

        const std::size_t start = static_cast<std::size_t>(take_position);
        mix_run(left_source + start, right_source + start, outputs, channels, offset, readable,
                clip, clip_frame, gains);
    }

    void TrackPlayer::mix_clip_at_speed(Track& track, const Clip& clip, float* const* outputs,
                                        std::uint32_t channels, std::uint32_t offset,
                                        std::uint32_t count, std::uint64_t clip_frame,
                                        std::uint64_t position, std::uint64_t speed_q32,
                                        MixGains gains) noexcept
    {
        const Take* const take = clip.take;

        if (take == nullptr || take->channels() == 0u || count == 0u)
        {
            return;
        }

        // The expected position may be one frame off; see modules/nodes/capture/README.md.
        const std::uint64_t drift = position > track.reader_position
                                        ? position - track.reader_position
                                        : track.reader_position - position;
        const bool continues = track.reader_primed && drift <= 1u &&
                               track.reader_clip_start == clip.start_frame &&
                               track.reader_speed_q32 == speed_q32;

        if (!continues)
        {
            track.reader.reset();
            track.reader_take_frame = clip.take_offset + clip_frame;
            track.reader_clip_start = clip.start_frame;
            track.reader_speed_q32 = speed_q32;
            track.reader_primed = true;
        }

        const std::uint32_t source_channels = std::min<std::uint32_t>(2u, take->channels());
        const double speed =
            static_cast<double>(speed_q32) / static_cast<double>(K_UNITY_SPEED_Q32);
        float* const pointers[2] = {scratch_[0].data(), scratch_[1].data()};

        track.reader_take_frame = track.reader.read(*take, track.reader_take_frame, speed, pointers,
                                                    source_channels, count);
        track.reader_position = position + distance_covered(count, speed_q32);

        const float* const left_source = scratch_[0].data();
        const float* const right_source = source_channels >= 2u ? scratch_[1].data() : left_source;

        mix_run(left_source, right_source, outputs, channels, offset, count, clip, clip_frame,
                gains);
    }

    void TrackPlayer::process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                              Transport& transport)
    {
        mix_lanes(0, K_MAX_TRACKS, true, outputs, channels, frames, transport);
    }

    void TrackPlayer::process_lane(std::size_t track, float* const* outputs,
                                   std::uint32_t channels, std::uint32_t frames,
                                   Transport& transport)
    {
        if (track < K_MAX_TRACKS)
        {
            mix_lanes(track, track + 1, false, outputs, channels, frames, transport);
        }
    }

    void TrackPlayer::mix_lanes(std::size_t first, std::size_t last, bool with_fader,
                                float* const* outputs, std::uint32_t channels,
                                std::uint32_t frames, Transport& transport)
    {
        DenormalGuard guard;

        if (outputs == nullptr || channels == 0 || frames == 0)
        {
            return;
        }

        std::array<TrackTimeline*, K_MAX_TRACKS> timelines{};
        bool any_soloed = false;
        for (std::size_t index = 0; index < K_MAX_TRACKS; ++index)
        {
            timelines[index] = tracks_[index].timeline.acquire_for_audio_thread();
            any_soloed = any_soloed || tracks_[index].soloed.load(std::memory_order_acquire);
        }

        std::uint64_t loop_begin = 0;
        std::uint64_t loop_length = 0;
        const bool looping = transport.loop_snapshot(loop_begin, loop_length);
        const std::uint64_t loop_end = loop_begin + loop_length;
        const std::uint64_t block_position = transport.position_frames();

        std::uint64_t speed_q32 = transport.speed_q32();

        // An unsized converter takes the copy path; see modules/nodes/capture/README.md.
        if (frames > prepared_block_frames_ || speed_q32 == 0u)
        {
            speed_q32 = K_UNITY_SPEED_Q32;
        }

        const bool unity = speed_q32 == K_UNITY_SPEED_Q32;

        for (std::size_t index = first; index < last; ++index)
        {
            Track& track = tracks_[index];
            const bool audible = !with_fader ||
                                 (any_soloed ? track.soloed.load(std::memory_order_acquire)
                                             : !track.muted.load(std::memory_order_acquire));
            if (!audible || timelines[index] == nullptr)
            {
                continue;
            }

            const TrackTimeline& lane = *timelines[index];
            const float pan_value = track.pan.load(std::memory_order_acquire);
            const float track_gain = track.gain.load(std::memory_order_acquire);
            const float theta = (pan_value + 1.0f) * K_QUARTER_PI;
            const MixGains gains = with_fader
                                       ? MixGains{std::cos(theta) * track_gain,
                                                  std::sin(theta) * track_gain, track_gain}
                                       : MixGains{1.0f, 1.0f, 1.0f};

            std::uint64_t position = block_position;
            std::uint32_t written = 0;
            while (written < frames)
            {
                std::uint32_t run = frames - written;
                if (looping && position < loop_end)
                {
                    run = frames_to_reach(loop_end - position, speed_q32, run);
                }

                const std::uint64_t run_span = distance_covered(run, speed_q32);
                const std::uint64_t run_end = position + run_span;

                for (std::size_t clip_index = lane.first_clip_at_or_after(position);
                     clip_index < lane.clip_count(); ++clip_index)
                {
                    const Clip& clip = lane.clip(clip_index);
                    const std::uint64_t clip_end = clip.start_frame + clip.length_frames;

                    if (clip.start_frame >= run_end)
                    {
                        break;
                    }

                    const std::uint64_t segment_begin = std::max(clip.start_frame, position);
                    const std::uint64_t segment_end = std::min(clip_end, run_end);
                    if (segment_end <= segment_begin)
                    {
                        continue;
                    }

                    const std::uint32_t begin_offset =
                        frames_to_reach(segment_begin - position, speed_q32, run);
                    const std::uint32_t end_offset =
                        frames_to_reach(segment_end - position, speed_q32, run);
                    if (end_offset <= begin_offset)
                    {
                        continue;
                    }

                    const std::uint64_t clip_frame = segment_begin - clip.start_frame;
                    const std::uint32_t count = end_offset - begin_offset;

                    if (unity)
                    {
                        mix_clip_at_unity(clip, outputs, channels, written + begin_offset, count,
                                          clip_frame, gains);
                    }
                    else
                    {
                        mix_clip_at_speed(track, clip, outputs, channels, written + begin_offset,
                                          count, clip_frame, segment_begin, speed_q32, gains);
                    }
                }

                position = run_end;
                written += run;
                if (looping && position >= loop_end && position >= loop_begin)
                {
                    position = loop_begin;
                }
            }
        }
    }
} // namespace SushiDSP
