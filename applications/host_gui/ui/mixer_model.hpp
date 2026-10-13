/**************************************************************************/
/* mixer_model.hpp                                                        */
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
 * @file mixer_model.hpp
 * @brief Declares the mixer's strips as data: what each is called and which slots hold its controls.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cabinet_section_wiring.hpp"
#include "rig_shape.hpp"
#include "ui/geometry.hpp"

namespace ui
{
    /// @brief What one strip of the mixer stands for.
    enum class MixerStripKind : std::uint8_t
    {
        Microphone,
        Room,
        Listener,
        Cabinet,
        Line,
        Bus,
        Di,
        Master
    };

    /// @brief Names where a strip's mute and solo flags live: one input of one bus.
    struct MixerFlags
    {
            SlotId bus = 0;        ///< The bus that holds the flags.
            std::size_t input = 0; ///< The input the strip feeds.
    };

    /// @brief One strip of the mixer: what it is called, which slot holds its level, and where its
    /// mute and solo live. Pure data.
    struct MixerStrip
    {
            MixerStripKind kind = MixerStripKind::Master; ///< What the strip stands for.
            std::string label;                 ///< The name drawn above the strip.
            std::optional<SlotId> strip;       ///< The slot that holds level and pan or balance.
            bool stereo = false;               ///< True when `strip` is a stereo strip.
            std::optional<SlotId> microphone;  ///< The microphone's own slot, for a Microphone strip.
            bool room_microphone = false;      ///< True when the microphone is summed on the Room strip.
            std::optional<MixerFlags> flags;   ///< Where mute and solo live; empty when there is none.
    };

    /// @brief Lists the mixer's strips in the order they are drawn: each microphone, the Room
    /// strip, the Listener, the join, the Bus, the DI and the Master. A strip whose slot is not
    /// built is left out. The join's strip is Cabinet when every path holds a cabinet, Line when
    /// none does, and Cabinet + Line when the chain has summed both.
    /// @param slots The rig's hidden slots, as the applier last wired them.
    /// @param join What the chain's join carries.
    /// @return The strips.
    std::vector<MixerStrip> mixer_strips(const HiddenSlots& slots,
                                         JoinContent join = JoinContent::Cabinets);
} // namespace ui
