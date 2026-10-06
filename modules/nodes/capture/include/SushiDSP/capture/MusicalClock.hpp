/**************************************************************************/
/* MusicalClock.hpp                                                       */
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
 * @file MusicalClock.hpp
 * @brief Declares the mapping between sample frames and bars and beats.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace SushiDSP
{
    /// @brief Names a point on the musical grid: the bar, the beat inside it and how far
    /// into that beat the point lies.
    struct BarBeat
    {
        std::uint64_t bar;      ///< Counts the bar from 1.
        std::uint32_t beat;     ///< Counts the beat inside the bar from 1.
        double        fraction; ///< Holds the position inside the beat, in [0, 1).
    };

    /// @brief Owns the mapping between sample frames and bars and beats, and nothing else.
    /// The control thread writes the tempo, the meter and the sample rate; each write
    /// republishes one 32.32 fixed-point frames-per-beat word that the audio thread reads
    /// with a single relaxed load, so no reader ever sees a torn tempo. The class holds no
    /// position: a caller passes a frame in, which is what keeps it detachable from
    /// Transport.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §2 and §3
    class MusicalClock
    {
    public:
        /// @brief Constructs a clock at 120 BPM in 4/4 at 48 kHz.
        MusicalClock() noexcept;

        MusicalClock(const MusicalClock&) = delete;
        MusicalClock& operator=(const MusicalClock&) = delete;

        /// @brief Sets the tempo and republishes the step. Control thread.
        /// @param beats_per_minute Quarter notes per minute.
        /// @pre beats_per_minute is finite and greater than zero; any other value leaves the
        /// clock as it was.
        void set_tempo(double beats_per_minute) noexcept;

        /// @brief Sets the meter and republishes the step. Control thread.
        /// @param beats_per_bar How many beats a bar holds.
        /// @param beat_unit The note value one beat is, as its denominator: 4 is a quarter,
        /// 8 an eighth.
        /// @pre Both arguments are greater than zero; any other pair leaves the clock as it
        /// was.
        void set_meter(std::uint32_t beats_per_bar, std::uint32_t beat_unit) noexcept;

        /// @brief Sets the sample rate and republishes the step. Control thread.
        /// @param sample_rate Frames per second.
        /// @pre sample_rate is finite and greater than zero; any other value leaves the clock
        /// as it was.
        void set_sample_rate(double sample_rate) noexcept;

        /// @brief Returns the frames per beat in 32.32 fixed point. Audio thread: one relaxed
        /// load, no allocation, no lock and no throw.
        std::uint64_t beat_step_q32() const noexcept;

        /// @brief Returns how many beats a bar holds.
        std::uint32_t beats_per_bar() const noexcept;

        /// @brief Returns the tempo in quarter notes per minute.
        double beats_per_minute() const noexcept;

        /// @brief Returns the note value one beat is, as its denominator.
        std::uint32_t beat_unit() const noexcept;

        /// @brief Returns the sample rate the step was computed at.
        double sample_rate() const noexcept;

        /// @brief Locates a frame on the musical grid. Control thread.
        /// @param frame The timeline frame to place.
        /// @return The bar, the beat and the fraction of a beat the frame falls on.
        BarBeat position_of(std::uint64_t frame) const noexcept;

        /// @brief Returns the first frame of a beat, derived from the beat's absolute index
        /// so that no error accumulates down the timeline. Control thread.
        /// @param bar The bar, counted from 1.
        /// @param beat The beat inside that bar, counted from 1.
        /// @pre Both arguments are at least 1; a zero is read as 1.
        std::uint64_t frame_of(std::uint64_t bar, std::uint32_t beat) const noexcept;

    private:
        /// @brief Recomputes the fixed-point step from the tempo, the meter and the sample
        /// rate, and publishes it.
        void republish_step() noexcept;

        std::atomic<std::uint64_t> beat_step_q32_;   ///< Holds the frames per beat in 32.32.
        double                     beats_per_minute_; ///< Holds the tempo in quarters a minute.
        double                     sample_rate_;      ///< Holds the frames per second.
        std::uint32_t              beats_per_bar_;    ///< Holds how many beats a bar has.
        std::uint32_t              beat_unit_;        ///< Holds the beat's note denominator.
    };
}
