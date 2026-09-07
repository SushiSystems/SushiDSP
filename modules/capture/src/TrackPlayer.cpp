/**************************************************************************/
/* TrackPlayer.cpp                                                        */
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

#include "SushiDSP/capture/TrackPlayer.hpp"

#include <algorithm>
#include <cmath>

#include "SushiDSP/core/DenormalGuard.hpp"

namespace SushiDSP
{
    namespace
    {
        constexpr float kQuarterPi = 0.78539816339744830961f;

        float clamp_pan(float pan)
        {
            return std::min(1.0f, std::max(-1.0f, pan));
        }
    }

    void TrackPlayer::set_take(std::size_t index, std::unique_ptr<Take> take)
    {
        if (index >= kMaxTracks || take == nullptr)
        {
            return;
        }
        tracks_[index].take.publish(std::move(take));
    }

    void TrackPlayer::set_gain(std::size_t index, float gain)
    {
        if (index < kMaxTracks)
        {
            tracks_[index].gain.store(gain, std::memory_order_release);
        }
    }

    void TrackPlayer::set_pan(std::size_t index, float pan)
    {
        if (index < kMaxTracks)
        {
            tracks_[index].pan.store(clamp_pan(pan), std::memory_order_release);
        }
    }

    void TrackPlayer::set_mute(std::size_t index, bool muted)
    {
        if (index < kMaxTracks)
        {
            tracks_[index].muted.store(muted, std::memory_order_release);
        }
    }

    void TrackPlayer::set_solo(std::size_t index, bool soloed)
    {
        if (index < kMaxTracks)
        {
            tracks_[index].soloed.store(soloed, std::memory_order_release);
        }
    }

    void TrackPlayer::reclaim()
    {
        for (Track& track : tracks_)
        {
            track.take.reclaim();
        }
    }

    float TrackPlayer::gain(std::size_t index) const
    {
        return index < kMaxTracks ? tracks_[index].gain.load(std::memory_order_acquire) : 0.0f;
    }

    float TrackPlayer::pan(std::size_t index) const
    {
        return index < kMaxTracks ? tracks_[index].pan.load(std::memory_order_acquire) : 0.0f;
    }

    bool TrackPlayer::muted(std::size_t index) const
    {
        return index < kMaxTracks && tracks_[index].muted.load(std::memory_order_acquire);
    }

    bool TrackPlayer::soloed(std::size_t index) const
    {
        return index < kMaxTracks && tracks_[index].soloed.load(std::memory_order_acquire);
    }

    void TrackPlayer::mix_run(const Take& take, float* const* outputs, std::uint32_t channels,
                              std::uint32_t offset, std::uint32_t count, std::uint64_t position,
                              float gain_left, float gain_right, float mono_gain) noexcept
    {
        const std::uint64_t available = take.frames() > position ? take.frames() - position : 0;
        const std::uint32_t readable =
            static_cast<std::uint32_t>(std::min<std::uint64_t>(count, available));
        if (readable == 0 || take.channels() == 0)
        {
            return;
        }

        const float* left_source = take.channel(0);
        const float* right_source = take.channels() >= 2 ? take.channel(1) : left_source;
        if (left_source == nullptr || right_source == nullptr)
        {
            return;
        }

        const bool        mono_source = take.channels() < 2;
        const std::size_t start = static_cast<std::size_t>(position);
        float* const      out_left = outputs[0];

        if (channels < 2)
        {
            // A mono output carries no pan, so only the take's own two channels fold together.
            for (std::uint32_t frame = 0; frame < readable; ++frame)
            {
                const float source = mono_source
                    ? left_source[start + frame]
                    : 0.5f * (left_source[start + frame] + right_source[start + frame]);
                out_left[offset + frame] += mono_gain * source;
            }
            return;
        }

        float* const out_right = outputs[1];
        for (std::uint32_t frame = 0; frame < readable; ++frame)
        {
            out_left[offset + frame] += left_source[start + frame] * gain_left;
            out_right[offset + frame] +=
                (mono_source ? left_source[start + frame] : right_source[start + frame])
                * gain_right;
        }
    }

    void TrackPlayer::process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                              Transport& transport)
    {
        DenormalGuard guard;

        if (outputs == nullptr || channels == 0 || frames == 0)
        {
            return;
        }

        std::array<Take*, kMaxTracks> takes{};
        bool                          any_soloed = false;
        for (std::size_t index = 0; index < kMaxTracks; ++index)
        {
            takes[index] = tracks_[index].take.acquire_for_audio_thread();
            any_soloed = any_soloed || tracks_[index].soloed.load(std::memory_order_acquire);
        }

        std::uint64_t       loop_begin = 0;
        std::uint64_t       loop_length = 0;
        const bool          looping = transport.loop_snapshot(loop_begin, loop_length);
        const std::uint64_t loop_end = loop_begin + loop_length;
        const std::uint64_t block_position = transport.position_frames();

        for (std::size_t index = 0; index < kMaxTracks; ++index)
        {
            const Track& track = tracks_[index];
            const bool   audible = any_soloed ? track.soloed.load(std::memory_order_acquire)
                                              : !track.muted.load(std::memory_order_acquire);
            if (!audible || takes[index] == nullptr)
            {
                continue;
            }

            const float pan_value = track.pan.load(std::memory_order_acquire);
            const float track_gain = track.gain.load(std::memory_order_acquire);
            const float theta = (pan_value + 1.0f) * kQuarterPi;
            const float gain_left = std::cos(theta) * track_gain;
            const float gain_right = std::sin(theta) * track_gain;

            std::uint64_t position = block_position;
            std::uint32_t written = 0;
            while (written < frames)
            {
                std::uint32_t run = frames - written;
                if (looping && position < loop_end)
                {
                    run = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(run, loop_end - position));
                }

                mix_run(*takes[index], outputs, channels, written, run, position, gain_left,
                        gain_right, track_gain);

                position += run;
                written += run;
                if (looping && position >= loop_end && position >= loop_begin)
                {
                    position = loop_begin;
                }
            }
        }
    }
}
