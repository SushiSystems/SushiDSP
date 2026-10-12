/**************************************************************************/
/* tap_tempo.hpp                                                          */
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
 * @file tap_tempo.hpp
 * @brief Declares the class that turns a run of taps into a tempo.
 * @author Mustafa Garip
 */

#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace ui
{
    /// @brief Turns a run of taps into a tempo. It holds the times of the last few taps and
    /// reports the tempo their spacing implies; a pause longer than K_MAXIMUM_GAP_SECONDS starts
    /// the run again, so a tap left over from a minute ago never drags the average.
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §6
    class TapTempo
    {
        public:
            /// @brief How many taps the average is taken over.
            static constexpr std::size_t K_TAP_COUNT = 4;

            /// @brief How long a gap between two taps may be before the run is started again, in
            /// seconds.
            static constexpr double K_MAXIMUM_GAP_SECONDS = 2.0;

            /// @brief The slowest tempo a run of taps may report, in quarter notes per minute.
            static constexpr double K_MINIMUM_TEMPO = 30.0;

            /// @brief The fastest tempo a run of taps may report, in quarter notes per minute.
            static constexpr double K_MAXIMUM_TEMPO = 300.0;

            /// @brief Records a tap and reports the tempo the run now implies.
            /// @param seconds The tap's time, on any clock that only moves forward.
            /// @return The tempo in quarter notes per minute, or std::nullopt while the run holds
            /// one tap and names no interval yet.
            std::optional<double> tap(double seconds);

            /// @brief Forgets every tap, so the next one starts a run of its own.
            void reset() noexcept;

            /// @brief Reports how many taps the current run holds.
            std::size_t tap_count() const noexcept;

        private:
            /// @brief Holds the times of the run's taps, oldest first.
            std::array<double, K_TAP_COUNT> taps_{};

            /// @brief Counts how many of taps_ the current run has filled.
            std::size_t count_{0};
    };
} // namespace ui
