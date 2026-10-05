/****************************************************************/
/* TransportBar.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
    /// @see docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §5a and §6
    /// @see docs/design/GUI_UX_REDESIGN.md §12
    void DrawTransportBar(RigController& rig, LevelProbe& input, LevelProbe& output,
                          bool& mini_daw_active, std::string& transport_error);
}
