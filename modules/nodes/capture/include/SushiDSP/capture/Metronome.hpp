/****************************************************************/
/* Metronome.hpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Metronome.hpp
 * @brief Declares the metronome that adds a click on every beat of the transport.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "SushiDSP/capture/MusicalClock.hpp"
#include "SushiDSP/capture/Transport.hpp"

namespace SushiDSP
{
    /// @brief Adds a click on every beat into an output buffer, reading its beats from a
    /// Transport position and a MusicalClock step. It is not an IAudioNode and it holds no
    /// place in the RigGraph; the host calls it after RecordingBus::write, so the click is
    /// heard on the monitor path and reaches no recorded file. Every beat is derived from the
    /// absolute position, so a seek moves the click with the playhead and nothing accumulates
    /// down a long take.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §4
    class Metronome
    {
    public:
        /// @brief Builds the two click bursts at 48 kHz.
        Metronome();

        /// @brief Builds the two click bursts at a sample rate.
        /// @param sample_rate Frames per second; a value that is not finite and greater than
        /// zero is read as 48000.
        explicit Metronome(double sample_rate);

        Metronome(const Metronome&) = delete;
        Metronome& operator=(const Metronome&) = delete;

        /// @brief Rebuilds the two click bursts at a new sample rate. Control thread;
        /// allocates.
        /// @param sample_rate Frames per second; a value that is not finite and greater than
        /// zero leaves the bursts as they were.
        void set_sample_rate(double sample_rate);

        /// @brief Returns the sample rate the bursts were built at.
        double sample_rate() const noexcept;

        /// @brief Turns the click on or off. Control thread.
        /// @param enabled True to let process() add anything at all.
        void set_enabled(bool enabled) noexcept;

        /// @brief Reports whether the click is on. Either thread.
        bool enabled() const noexcept;

        /// @brief Sets the linear gain every click sample is scaled by. Control thread.
        /// @param level The linear gain; a value below zero is read as zero.
        void set_level(float level) noexcept;

        /// @brief Reports the linear gain every click sample is scaled by. Either thread.
        float level() const noexcept;

        /// @brief Adds a click at every beat frame the block covers, continuing whatever burst
        /// the previous block left running. Adds nothing when the click is off or the transport
        /// is stopped. Audio thread. Real-time safe: allocation-free, lock-free and throw-free.
        /// @param outputs One writable buffer of at least frames samples per output channel.
        /// @param channels How many output channels outputs holds.
        /// @param frames How many frames the block covers.
        /// @param transport The clock giving the block's first frame and the playing state.
        /// @param clock The grid giving the beat step and how many beats a bar holds.
        void process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                     const Transport& transport, const MusicalClock& clock) noexcept;

        /// @brief How long a click lasts, in seconds.
        static constexpr double kBurstSeconds = 0.030;

        /// @brief How long a click's raised-cosine attack lasts, in seconds.
        static constexpr double kAttackSeconds = 0.001;

        /// @brief The time constant of a click's exponential decay, in seconds.
        static constexpr double kDecaySeconds = 0.008;

        /// @brief The frequency of the click that marks the first beat of a bar, in Hz.
        static constexpr double kDownbeatHertz = 1760.0;

        /// @brief The frequency of every other beat's click, in Hz.
        static constexpr double kBeatHertz = 880.0;

    private:
        /// @brief Fills one burst vector with a decaying sine at a frequency.
        /// @param burst The vector to size and fill.
        /// @param frequency_hertz The sine's frequency in Hz.
        void build_burst(std::vector<float>& burst, double frequency_hertz);

        /// @brief Returns the first frame of a beat, by the same arithmetic MusicalClock uses,
        /// so a click lands on the frame the ruler draws its beat line at.
        /// @param index The beat counted from zero at frame zero.
        /// @param step The frames per beat in 32.32 fixed point.
        static std::uint64_t beat_frame(std::uint64_t index, std::uint64_t step) noexcept;

        /// @brief Returns the lowest beat index whose frame is at or after a position.
        /// @param position The timeline frame the block starts at.
        /// @param step The frames per beat in 32.32 fixed point.
        /// @pre step is not zero.
        static std::uint64_t first_beat_at_or_after(std::uint64_t position,
                                                    std::uint64_t step) noexcept;

        /// @brief Adds the running burst into every channel from an output offset onward, and
        /// leaves burst_position_ on the first burst sample the next block owes.
        /// @param outputs The output buffers to add into.
        /// @param channels How many output channels outputs holds.
        /// @param offset The first output frame this run writes.
        /// @param frames How many frames the block covers.
        /// @param gain The linear gain every burst sample is scaled by.
        void render_burst(float* const* outputs, std::uint32_t channels, std::uint32_t offset,
                          std::uint32_t frames, float gain) noexcept;

        /// @brief Holds the downbeat click, one burst of samples.
        std::vector<float> downbeat_burst_;

        /// @brief Holds every other beat's click, one burst of samples.
        std::vector<float> beat_burst_;

        /// @brief Holds the rate the bursts were built at, in frames per second.
        double             sample_rate_;

        /// @brief Holds the next burst sample a block owes, or the burst's length when no
        /// burst is running. Audio thread only.
        std::size_t        burst_position_;

        /// @brief Marks the running burst as the downbeat's. Audio thread only.
        bool               burst_is_downbeat_;

        /// @brief Holds whether process() adds anything at all.
        std::atomic<bool>  enabled_{ false };

        /// @brief Holds the linear gain every click sample is scaled by.
        std::atomic<float> level_{ 0.5f };
    };
}
