/**************************************************************************/
/* musical_time_text.hpp                                                  */
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
 * @file musical_time_text.hpp
 * @brief Declares the bar and beat text a timeline frame is named by.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <string>

#include "SushiDSP/capture/musical_clock.hpp"

namespace ui
{
    /// @brief How many parts a beat is divided into for the transport's readout.
    inline constexpr std::uint32_t K_SIXTEENTHS_PER_BEAT = 4;

    /// @brief Names a frame by its bar and its beat, for a clip's label.
    /// @param frame The timeline frame to name.
    /// @param clock The grid the bar and the beat are read from.
    /// @return The text, e.g. "9.1".
    std::string bar_beat_text(std::uint64_t frame, const SushiDSP::MusicalClock& clock);

    /// @brief Names a frame by its bar, its beat and the sixteenth inside that beat, for the
    /// transport's position readout. Bars, beats and sixteenths are all counted from 1.
    /// @param frame The timeline frame to name.
    /// @param clock The grid the three numbers are read from.
    /// @return The text, e.g. "100 · 1 · 1".
    std::string bar_beat_sixteenth_text(std::uint64_t frame, const SushiDSP::MusicalClock& clock);
} // namespace ui
