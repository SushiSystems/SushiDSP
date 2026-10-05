/****************************************************************/
/* Transport.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Transport.hpp
 * @brief Declares the sample clock every track shares.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace SushiDSP
{
    /// @brief What the transport is doing right now.
    enum class TransportState : std::uint32_t
    {
        Stopped = 0,
        Playing = 1,
        Recording = 2
    };

    /// @brief The sample clock every track shares: a frame position, a state, and an
    /// optional loop range. The audio thread advances the position once per block and the
    /// control thread moves it, all through atomics, so neither side ever waits on the
    /// other. The loop range lives in one 64-bit word, which bounds a loop to the first
    /// 2^32 frames of the timeline.
    /// @see docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md §6
    class Transport
    {
    public:
        Transport() = default;

        Transport(const Transport&) = delete;
        Transport& operator=(const Transport&) = delete;

        /// @brief Moves the position on by the block's frames times the playback speed,
        /// wrapping to the loop begin when the block crosses the loop end. Audio thread.
        /// Real-time safe: three atomic operations, no allocation and no lock. The fraction
        /// a block leaves over is carried in 32.32 fixed point, so the position after any
        /// number of blocks is the exact floor of frames times speed and never drifts.
        /// @param frames How many output frames the block covered.
        void advance(std::uint32_t frames) noexcept;

        /// @brief Puts the transport into the Playing state, leaving the position alone.
        /// Control thread.
        void play() noexcept;

        /// @brief Puts the transport into the Stopped state, leaving the position alone.
        /// Control thread.
        void stop() noexcept;

        /// @brief Puts the transport into the Recording state, leaving the position alone.
        /// Control thread.
        void record() noexcept;

        /// @brief Moves the position to an arbitrary frame. Control thread.
        /// @param frames The frame the transport should read from next.
        /// @pre A seek concurrent with advance() may be overwritten by that block's store; the
        /// host serialises them.
        void seek(std::uint64_t frames) noexcept;

        /// @brief Sets the loop range the audio thread wraps at. Control thread.
        /// @param begin_frames The frame the loop returns to.
        /// @param end_frames The first frame past the loop.
        /// @pre end_frames is greater than begin_frames and both fit in 32 bits; a range
        /// that fails either clears the loop instead of setting one.
        void set_loop(std::uint64_t begin_frames, std::uint64_t end_frames) noexcept;

        /// @brief Removes the loop range, so the position runs on without wrapping.
        /// Control thread.
        void clear_loop() noexcept;

        /// @brief The frame the next block starts at. Either thread.
        /// @return The current position in frames.
        std::uint64_t position_frames() const noexcept;

        /// @brief What the transport is doing. Either thread.
        /// @return The current state.
        TransportState state() const noexcept;

        /// @brief Whether a loop range is set. Either thread.
        /// @return True when set_loop() established a range that clear_loop() has not
        /// removed.
        bool has_loop() const noexcept;

        /// @brief The frame a wrap returns to. Either thread.
        /// @return The loop begin, or 0 when no loop is set.
        std::uint64_t loop_begin_frames() const noexcept;

        /// @brief The first frame past the loop. Either thread.
        /// @return The loop end, or 0 when no loop is set.
        std::uint64_t loop_end_frames() const noexcept;

        /// @brief Reads the whole loop range out of one atomic load, so a block can never mix
        /// one range's begin with another's end. Either thread. Real-time safe.
        /// @param out_begin Receives the loop begin in frames, 0 when no loop is set.
        /// @param out_length Receives the loop length in frames, 0 when no loop is set.
        /// @return True when a loop range is set.
        bool loop_snapshot(std::uint64_t& out_begin, std::uint64_t& out_length) const noexcept;

        /// @brief Sets how fast the playhead moves, in timeline frames per output frame.
        /// Clamped to [kMinSpeed, kMaxSpeed]. Control thread.
        /// @param speed The requested playback speed.
        void set_speed(double speed) noexcept;

        /// @brief The playback speed in 32.32 fixed point. Audio thread; one relaxed load.
        /// @return The speed as a 32.32 fixed-point word.
        std::uint64_t speed_q32() const noexcept;

        /// @brief The playback speed. Control thread; for the UI.
        /// @return The speed as a double.
        double speed() const noexcept;

        /// @brief The slowest playback speed set_speed() accepts.
        static constexpr double kMinSpeed = 0.5;

        /// @brief The fastest playback speed set_speed() accepts.
        static constexpr double kMaxSpeed = 2.0;

    private:
        /// @brief Splits the packed loop word into its begin frame and length.
        /// @param packed The word loop_ holds.
        /// @param out_begin Receives the loop begin in frames.
        /// @param out_length Receives the loop length in frames, 0 when no loop is set.
        static void unpack_loop(std::uint64_t packed, std::uint64_t& out_begin,
                                std::uint64_t& out_length) noexcept;

        /// @brief Holds the frame the next block starts at.
        std::atomic<std::uint64_t>  position_{ 0 };

        /// @brief Holds what the transport is doing right now.
        std::atomic<TransportState> state_{ TransportState::Stopped };

        /// @brief Holds the loop begin in the high half and the loop length in the low half.
        std::atomic<std::uint64_t>  loop_{ 0 };

        /// @brief Holds the playback speed in 32.32 fixed point; unity is 1ull << 32.
        std::atomic<std::uint64_t>  speed_q32_{ 1ull << 32 };

        /// @brief Holds the fractional frame carried between blocks. Audio thread only.
        std::uint64_t               speed_remainder_{ 0 };
    };
}
