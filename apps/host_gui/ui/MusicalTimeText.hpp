/**************************************************************************/
/* MusicalTimeText.hpp                                                    */
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
