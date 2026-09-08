/**************************************************************************/
/* TransportBar.hpp                                                       */
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

#include <string>

#include "RigController.hpp"

namespace ui
{
    /// @brief Draws the take folder and name boxes, the record, play, stop and rewind buttons,
    /// the loop switch and its range in bars, the rig's input source, the transport position in
    /// bars and in seconds, the metronome's switch and level, the tempo, the meter, TAP, the
    /// playback speed and the recorder's dropped-block count into the current window.
    /// @param rig The rig whose transport, musical clock, metronome, recording bus and input
    /// source the bar drives.
    /// @param transport_error In/out: the last refusal, shown until an action succeeds.
    /// @see docs/superpowers/specs/2026-09-08-arrangement-view-design.md §5a and §6
    void DrawTransportBar(RigController& rig, std::string& transport_error);
}
