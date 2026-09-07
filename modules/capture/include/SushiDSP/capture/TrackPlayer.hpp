/**************************************************************************/
/* TrackPlayer.hpp                                                        */
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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "SushiDSP/capture/Take.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/core/RtSwapSlot.hpp"

namespace SushiDSP
{
    /// @brief Mixes up to kMaxTracks recorded takes into one output at the transport's
    /// position. Each track carries its own take, gain, pan, mute and solo; the control thread
    /// sets all five and the audio thread only reads them, so a take can be swapped under a
    /// running mix without a lock. process() adds into the output rather than overwriting it, so
    /// the caller clears or fills the buffer first.
    /// @see docs/superpowers/specs/2026-09-07-stereo-tracks-design.md §6
    class TrackPlayer
    {
    public:
        /// @brief Caps how many tracks the player mixes.
        static constexpr std::size_t kMaxTracks = 8;

        TrackPlayer() = default;

        TrackPlayer(const TrackPlayer&) = delete;
        TrackPlayer& operator=(const TrackPlayer&) = delete;

        /// @brief Publishes a take to a track, replacing whatever it held. Control thread.
        /// @param index Track index; out of range does nothing.
        /// @param take The take to play, already fully built. Null does nothing.
        void set_take(std::size_t index, std::unique_ptr<Take> take);

        /// @brief Sets a track's linear gain. Control thread.
        /// @param index Track index; out of range does nothing.
        /// @param gain The linear gain applied to every sample of the track.
        void set_gain(std::size_t index, float gain);

        /// @brief Sets a track's pan position. Control thread.
        /// @param index Track index; out of range does nothing.
        /// @param pan -1 hard left, 0 centre, +1 hard right; clamped to that range.
        void set_pan(std::size_t index, float pan);

        /// @brief Silences or unsilences a track. Control thread.
        /// @param index Track index; out of range does nothing.
        /// @param muted True to drop the track from the mix.
        void set_mute(std::size_t index, bool muted);

        /// @brief Marks a track as soloed. Control thread.
        /// @param index Track index; out of range does nothing.
        /// @param soloed True to make this track one of the only ones heard.
        void set_solo(std::size_t index, bool soloed);

        /// @brief Frees every take the audio thread has provably moved past. Control thread;
        /// call it periodically, for instance once per GUI frame.
        void reclaim();

        /// @brief Adds every audible track into the output at the transport's position - the
        /// soloed tracks if any is soloed, otherwise the unmuted ones - wrapping at the loop end
        /// exactly as Transport::advance() will, and leaving the transport where it found it. A
        /// mono output carries no pan, so a track folds into it at its own gain whatever its pan
        /// position. Audio thread. Real-time safe: allocation-free, lock-free and throw-free.
        /// @param outputs One writable buffer of at least frames samples per output channel.
        /// @param channels How many output channels outputs holds.
        /// @param frames How many frames to mix.
        /// @param transport The clock giving the read position and the loop range.
        void process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                     Transport& transport);

        /// @brief Reports a track's gain. Either thread.
        /// @param index Track index.
        /// @return The linear gain, or 0 for an index out of range.
        float gain(std::size_t index) const;

        /// @brief Reports a track's pan position. Either thread.
        /// @param index Track index.
        /// @return The pan in [-1, 1], or 0 for an index out of range.
        float pan(std::size_t index) const;

        /// @brief Reports whether a track is muted. Either thread.
        /// @param index Track index.
        /// @return True when the track is muted; false for an index out of range.
        bool muted(std::size_t index) const;

        /// @brief Reports whether a track is soloed. Either thread.
        /// @param index Track index.
        /// @return True when the track is soloed; false for an index out of range.
        bool soloed(std::size_t index) const;

    private:
        /// @brief One track's take and its four mix controls.
        struct Track
        {
            /// @brief Holds the take the audio thread reads, swappable under a running mix.
            RtSwapSlot<Take>   take;

            /// @brief Holds the linear gain applied to every sample of the track.
            std::atomic<float> gain{ 1.0f };

            /// @brief Holds the pan position in [-1, 1].
            std::atomic<float> pan{ 0.0f };

            /// @brief Marks the track as dropped from the mix.
            std::atomic<bool>  muted{ false };

            /// @brief Marks the track as one of the only ones heard.
            std::atomic<bool>  soloed{ false };
        };

        /// @brief Adds one take's samples into the output over a contiguous run of frames.
        /// @param take The take to read, already acquired for this block.
        /// @param outputs The output buffers to add into.
        /// @param channels How many output channels outputs holds.
        /// @param offset The first output frame this run writes.
        /// @param count How many frames this run covers.
        /// @param position The take frame the run starts at; frames past the end add nothing.
        /// @param gain_left The left gain, pan and track gain folded in; a stereo output only.
        /// @param gain_right The right gain, pan and track gain folded in; a stereo output only.
        /// @param mono_gain The track gain alone, used when the output has no right channel.
        static void mix_run(const Take& take, float* const* outputs, std::uint32_t channels,
                            std::uint32_t offset, std::uint32_t count, std::uint64_t position,
                            float gain_left, float gain_right, float mono_gain) noexcept;

        /// @brief Holds every track's take and mix controls.
        std::array<Track, kMaxTracks> tracks_;
    };
}
