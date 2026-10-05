/****************************************************************/
/* MusicalTimeText.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MusicalTimeText.hpp
 * @brief Declares the bar and beat text a timeline frame is named by.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <string>

#include "SushiDSP/capture/MusicalClock.hpp"

namespace ui
{
    /// @brief How many parts a beat is divided into for the transport's readout.
    inline constexpr std::uint32_t kSixteenthsPerBeat = 4;

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
    std::string bar_beat_sixteenth_text(std::uint64_t frame,
                                        const SushiDSP::MusicalClock& clock);
}
