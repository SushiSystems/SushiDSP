/**************************************************************************/
/* track_player.hpp                                                       */
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
 * @file track_player.hpp
 * @brief Declares the mixer that plays clip timelines at the transport's position and speed.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/capture/take_library.hpp"
#include "SushiDSP/capture/track_timeline.hpp"
#include "SushiDSP/capture/transport.hpp"
#include "SushiDSP/capture/vari_rate_reader.hpp"
#include "SushiDSP/core/rt_swap_slot.hpp"

namespace SushiDSP
{
    /// @brief Mixes up to K_MAX_TRACKS clip timelines into one output at the transport's position
    /// and the transport's playback speed. Each lane carries its own timeline, gain, pan, mute
    /// and solo; the control thread sets all five and the audio thread only reads them, so a
    /// lane can be replaced under a running mix without a lock. process() adds into the output
    /// rather than overwriting it, so the caller clears or fills the buffer first.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md sections 5 and 5a
    class TrackPlayer
    {
        public:
            /// @brief Caps how many tracks the player mixes.
            static constexpr std::size_t K_MAX_TRACKS = 8;

            /// @brief Caps the block size prepare() sizes the speed converters for.
            static constexpr std::uint32_t K_MAX_BLOCK_FRAMES = 4096u;

            /// @brief Marks the playback speed at which a clip is copied rather than converted.
            static constexpr std::uint64_t K_UNITY_SPEED_Q32 = 1ull << 32;

            TrackPlayer() = default;

            TrackPlayer(const TrackPlayer&) = delete;
            TrackPlayer& operator=(const TrackPlayer&) = delete;

            /// @brief Sizes every lane's speed converter for the largest block process() will carry.
            /// Control thread; allocates.
            /// @param max_block_frames Largest frames a single process() will ask for; anything above
            /// K_MAX_BLOCK_FRAMES is capped there.
            void prepare(std::uint32_t max_block_frames);

            /// @brief Returns the library owning every take a lane's clips borrow. Control thread.
            TakeLibrary& takes() noexcept;

            /// @brief Replaces one lane's clips. Control thread; publishes through the lane's slot.
            /// @param track Lane index; out of range does nothing.
            /// @param timeline The lane's new clips. Null does nothing.
            void set_timeline(std::size_t track, std::unique_ptr<TrackTimeline> timeline);

            /// @brief Publishes one whole take to a lane as a single clip starting at frame 0. The
            /// take is handed to takes() and the clip borrows it back from there. Control thread.
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

            /// @brief Frees what the audio thread has provably moved past. Reclaims every lane slot
            /// first and only then collects the library: the other order would free a take a retired
            /// timeline still names. Control thread; call it periodically, for instance once per GUI
            /// frame.
            /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md section 5
            void reclaim();

            /// @brief Adds every audible lane's clips into the output at the transport's position.
            /// @param outputs One writable buffer of at least frames samples per output channel.
            /// @pre prepare() has been called with at least frames whenever the speed is not unity.
            /// @see modules/nodes/capture/README.md, "What `TrackPlayer::process` mixes"
            void process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                         Transport& transport);

            /// @brief Adds one lane's clips into the output at the transport's position, before the
            /// lane's fader: at unity gain, a mono take on every channel, whatever the lane's gain,
            /// pan, mute and solo say. This is what feeds a lane into a rig of its own.
            /// @param track Lane index; out of range adds nothing.
            /// @param outputs One writable buffer of at least frames samples per output channel.
            /// @pre Each lane is rendered at most once a block, by this call or by process().
            void process_lane(std::size_t track, float* const* outputs, std::uint32_t channels,
                              std::uint32_t frames, Transport& transport);

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
            /// @brief One lane's clips, its speed converter and its four mix controls.
            struct Track
            {
                    /// @brief Holds the clips the audio thread reads, swappable under a running mix.
                    RtSwapSlot<TrackTimeline> timeline;

                    /// @brief Converts a clip to the transport's speed whenever that speed is not unity.
                    VariRateReader reader;

                    /// @brief Holds the take frame the converter continues from. Audio thread only.
                    std::uint64_t reader_take_frame{0};

                    /// @brief Holds the timeline frame the converter's next output belongs to, which is
                    /// what a discontinuity is detected against. Audio thread only.
                    std::uint64_t reader_position{0};

                    /// @brief Holds the start frame of the clip the converter is inside. Audio thread only.
                    std::uint64_t reader_clip_start{0};

                    /// @brief Holds the speed the converter's history was filled at. Audio thread only.
                    std::uint64_t reader_speed_q32{0};

                    /// @brief Marks the converter as holding history a continuation may reuse. Audio
                    /// thread only.
                    bool reader_primed{false};

                    /// @brief Holds the linear gain applied to every sample of the track.
                    std::atomic<float> gain{1.0f};

                    /// @brief Holds the pan position in [-1, 1].
                    std::atomic<float> pan{0.0f};

                    /// @brief Marks the track as dropped from the mix.
                    std::atomic<bool> muted{false};

                    /// @brief Marks the track as one of the only ones heard.
                    std::atomic<bool> soloed{false};
            };

            /// @brief Carries the three gains one lane mixes at, so a run signature stays short.
            struct MixGains
            {
                    float
                        left; ///< Holds the left gain with pan and track gain folded in; stereo only.
                    float
                        right; ///< Holds the right gain with pan and track gain folded in; stereo only.
                    float
                        mono; ///< Holds the track gain alone, used when the output has no right channel.
            };

            /// @brief Adds the lanes in [first, last) into the output. With the fader, a lane is
            /// mixed at its gain and pan when mute and solo leave it audible; without, every lane
            /// in the range is added at unity.
            void mix_lanes(std::size_t first, std::size_t last, bool with_fader,
                           float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                           Transport& transport);

            /// @brief Returns a clip's de-click gain, leaving flat whichever edge is the take's own
            /// boundary rather than a cut. The ramp shape stays TrackTimeline::edge_gain's and is
            /// never restated here; suppressing an edge only moves the frame the ramp is asked about.
            /// @param clip The clip the frame belongs to.
            /// @param clip_frame How far into the clip the frame lies, in frames.
            /// @return A gain in [0, 1].
            static float clip_gain(const Clip& clip, std::uint64_t clip_frame) noexcept;

            /// @brief Returns how many output frames the position needs to cover a distance.
            /// @param distance Timeline frames to cover.
            /// @param speed_q32 Timeline frames per output frame, in 32.32 fixed point.
            /// @param remaining Output frames still available; the result never exceeds it.
            /// @return The output frame count, clamped to remaining.
            static std::uint32_t frames_to_reach(std::uint64_t distance, std::uint64_t speed_q32,
                                                 std::uint32_t remaining) noexcept;

            /// @brief Returns the timeline distance a run of output frames covers.
            /// @param output_frames Output frames the run holds.
            /// @param speed_q32 Timeline frames per output frame, in 32.32 fixed point.
            /// @return The distance in timeline frames.
            static std::uint64_t distance_covered(std::uint32_t output_frames,
                                                  std::uint64_t speed_q32) noexcept;

            /// @brief Adds one clip's samples into the output over a contiguous run of frames.
            /// @param left_source The left samples to read, starting at the run's first frame.
            /// @param right_source The right samples to read, starting at the run's first frame.
            /// @param offset The first output frame this run writes.
            /// @param clip The clip the run belongs to, which carries its de-click ramp.
            /// @param clip_frame How far into the clip the run's first frame lies.
            /// @param gains The lane's three mix gains.
            static void mix_run(const float* left_source, const float* right_source,
                                float* const* outputs, std::uint32_t channels, std::uint32_t offset,
                                std::uint32_t count, const Clip& clip, std::uint64_t clip_frame,
                                MixGains gains) noexcept;

            /// @brief Adds one clip's samples into the output by copying the take, which is what
            /// makes unity speed carry the take's own samples bit for bit.
            /// @param offset The first output frame this run writes.
            /// @param count How many frames this run covers.
            /// @param clip_frame How far into the clip the run's first frame lies.
            /// @param gains The lane's three mix gains.
            static void mix_clip_at_unity(const Clip& clip, float* const* outputs,
                                          std::uint32_t channels, std::uint32_t offset,
                                          std::uint32_t count, std::uint64_t clip_frame,
                                          MixGains gains) noexcept;

            /// @brief Adds one clip's samples into the output through the lane's speed converter,
            /// resetting that converter whenever the run does not continue the one before it.
            /// @param track The lane being mixed, whose converter carries the history.
            /// @param offset The first output frame this run writes.
            /// @param clip_frame How far into the clip the run's first frame lies.
            /// @param position The timeline frame the run's first output frame belongs to.
            /// @param speed_q32 Timeline frames per output frame, in 32.32 fixed point.
            void mix_clip_at_speed(Track& track, const Clip& clip, float* const* outputs,
                                   std::uint32_t channels, std::uint32_t offset,
                                   std::uint32_t count, std::uint64_t clip_frame,
                                   std::uint64_t position, std::uint64_t speed_q32,
                                   MixGains gains) noexcept;

            /// @brief Owns every take the lanes' clips borrow. Declared before tracks_ so that it
            /// outlives every TrackTimeline whose destructor releases into it.
            TakeLibrary takes_;

            /// @brief Holds every lane's clips, converter and mix controls.
            std::array<Track, K_MAX_TRACKS> tracks_;

            /// @brief Receives one converted run before it is mixed, so no lane allocates.
            std::array<std::array<float, K_MAX_BLOCK_FRAMES>, 2> scratch_{};

            /// @brief Holds the largest block prepare() sized the converters for.
            std::uint32_t prepared_block_frames_{0};
    };
} // namespace SushiDSP
