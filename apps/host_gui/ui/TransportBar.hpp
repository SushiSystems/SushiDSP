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
#include "ui/LevelProbe.hpp"

namespace ui
{
    /// @brief Draws the bottom transport bar into the current window: record/play/stop/rewind,
    /// the loop switch, the transport position, the metronome's switch, the tempo, the time
    /// signature, TAP, the playback speed, the disabled Pitch field, the Mini-DAW toggle and,
    /// pinned to the right, the input/output level meters. The take folder/name boxes, the rig's
    /// input source, the loop range, the metronome's level and the recorder's dropped-block count
    /// are not in the mockup this bar follows; they stay, wrapped into a second row after Speed.
    /// @param rig The rig whose transport, musical clock, metronome, recording bus and input
    /// source the bar drives.
    /// @param input The probe fed from the device's input block, for the right-hand meters.
    /// @param output The probe fed from the rig's output block, for the right-hand meters.
    /// @param mini_daw_active In/out: whether the workspace shows the mini-DAW; the bar's button
    /// toggles it and reads "Mini-DAW" or "Back to Rig" accordingly.
    /// @param transport_error In/out: the last refusal, shown until an action succeeds.
    /// @see docs/superpowers/specs/2026-09-08-arrangement-view-design.md §5a and §6
    /// @see docs/design/gui_ux_redesign.md §12
    void DrawTransportBar(RigController& rig, LevelProbe& input, LevelProbe& output,
                          bool& mini_daw_active, std::string& transport_error);
}
