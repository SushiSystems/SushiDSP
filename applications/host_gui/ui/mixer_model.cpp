/**************************************************************************/
/* mixer_model.cpp                                                        */
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
 * @file mixer_model.cpp
 * @brief Implements the list of the mixer's strips.
 * @author Mustafa Garip & Sushi Systems
 */

#include "ui/mixer_model.hpp"

#include <algorithm>

namespace ui
{
    namespace
    {
        /// @brief Finds which input of a bus a feeder holds.
        /// @return The flags' place, or nullopt when the bus is not built or the feeder is not
        /// one of its inputs.
        std::optional<MixerFlags> flags_of(const std::optional<SlotId>& bus,
                                           const std::vector<SlotId>& feeders,
                                           const std::optional<SlotId>& feeder)
        {
            if (!bus.has_value() || !feeder.has_value())
            {
                return std::nullopt;
            }
            const auto found = std::find(feeders.begin(), feeders.end(), *feeder);
            if (found == feeders.end())
            {
                return std::nullopt;
            }
            return MixerFlags{*bus, static_cast<std::size_t>(found - feeders.begin())};
        }
    } // namespace

    std::vector<MixerStrip> mixer_strips(const HiddenSlots& slots)
    {
        std::vector<MixerStrip> strips;
        if (!slots.amp_bus.has_value())
        {
            return strips;
        }

        for (std::size_t index = 0; index < slots.microphones.size(); ++index)
        {
            const MicrophoneSlots& microphone = slots.microphones[index];
            MixerStrip strip;
            strip.kind = MixerStripKind::Microphone;
            strip.label = "Mic " + std::to_string(index + 1);
            strip.strip = microphone.strip;
            strip.microphone = microphone.microphone;
            // A microphone feeds the bus, or the Room sum when it is marked room.
            strip.flags = flags_of(slots.bus, slots.bus_feeders, microphone.strip);
            if (!strip.flags.has_value())
            {
                strip.flags = flags_of(slots.room_sum, slots.room_sum_feeders, microphone.strip);
                strip.room_microphone = strip.flags.has_value();
            }
            strips.push_back(strip);
        }

        if (slots.room_strip.has_value())
        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Room;
            strip.label = "Room";
            strip.strip = slots.room_strip;
            strip.stereo = true;
            strip.flags = flags_of(slots.bus, slots.bus_feeders, slots.room_strip);
            strips.push_back(strip);
        }

        if (slots.listener_strip.has_value())
        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Listener;
            strip.label = "Listener";
            strip.strip = slots.listener_strip;
            strip.stereo = true;
            strip.flags = flags_of(slots.amp_bus, slots.amp_bus_feeders, slots.listener_strip);
            strips.push_back(strip);
        }

        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Cabinet;
            strip.label = "Cabinet";
            strip.flags = flags_of(slots.amp_bus, slots.amp_bus_feeders, slots.join);
            strips.push_back(strip);
        }

        if (slots.bus.has_value())
        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Bus;
            strip.label = "Bus";
            strip.flags = flags_of(slots.amp_bus, slots.amp_bus_feeders, slots.bus);
            strips.push_back(strip);
        }

        if (slots.di_strip.has_value())
        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Di;
            strip.label = "DI";
            strip.strip = slots.di_strip;
            // The DI joins the amp bus before the effects, or the master bus after them.
            strip.flags = flags_of(slots.amp_bus, slots.amp_bus_feeders, slots.di_strip);
            if (!strip.flags.has_value())
            {
                strip.flags =
                    flags_of(slots.master_bus, slots.master_bus_feeders, slots.di_strip);
            }
            strips.push_back(strip);
        }

        if (slots.master_strip.has_value())
        {
            MixerStrip strip;
            strip.kind = MixerStripKind::Master;
            strip.label = "Master";
            strip.strip = slots.master_strip;
            strip.stereo = true;
            strips.push_back(strip);
        }
        return strips;
    }
} // namespace ui
