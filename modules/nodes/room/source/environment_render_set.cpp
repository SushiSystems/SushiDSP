/**************************************************************************/
/* environment_render_set.cpp                                             */
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
 * @file environment_render_set.cpp
 * @brief Implements the environment render set's builder and the direct-delay compensation.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/environment_render_set.hpp"

#include <algorithm>
#include <cstddef>

namespace SushiDSP::Room
{
    void compensate_direct_delay(room::RoomRenderSet& set) noexcept
    {
        if (set.listener != room::ListenerKind::Ears || set.early.count == 0)
        {
            return;
        }
        const float shift = set.direct_delay_frames;
        // Tap 0 may land under a frame: the direct path reads it past the alignment delay.
        set.early.taps[0].delay_frames -= shift;
        for (std::uint32_t index = 1; index < set.early.count; ++index)
        {
            room::EarlyTap& tap = set.early.taps[index];
            tap.delay_frames = std::max(tap.delay_frames - shift, 1.0f);
        }
        set.late.delay_to_crossover_frames =
            std::max(set.late.delay_to_crossover_frames - shift, 0.0f);
    }

    std::unique_ptr<EnvironmentRenderSet>
    EnvironmentBuilder::build(const Arrangement& arrangement, const room::HrtfSet& hrtf,
                              double sample_rate, bool physical_delay,
                              const EnvironmentRenderSet* previous)
    {
        if (sample_rate != sample_rate_)
        {
            invalidate();
        }
        auto set = std::make_unique<EnvironmentRenderSet>();
        const std::size_t sources = arrangement.sources().size();
        const std::size_t receivers = 1 + arrangement.microphones().size();
        decltype(keys_) keys{};

        for (std::size_t source = 0; source < sources; ++source)
        {
            for (std::size_t receiver = 0; receiver < receivers; ++receiver)
            {
                Key& key = keys[source][receiver];
                key.scene = arrangement.pair_scene(source, receiver);
                key.physical = physical_delay;
                key.valid = true;

                const Key& before = keys_[source][receiver];
                const bool reusable =
                    previous != nullptr && before.valid && before.physical == physical_delay &&
                    source < previous->source_count &&
                    receiver <= previous->microphone_count &&
                    scenes_equal(before.scene, key.scene);
                if (reusable)
                {
                    set->pairs[source][receiver] = previous->pairs[source][receiver];
                    continue;
                }

                const room::ListenerKind kind = receiver == 0 ? room::ListenerKind::Ears
                                                              : room::ListenerKind::Microphone;
                const std::unique_ptr<room::RoomRenderSet> pair =
                    room::build_render_set(key.scene, hrtf, sample_rate, kind);
                if (!physical_delay)
                {
                    compensate_direct_delay(*pair);
                }
                set->pairs[source][receiver] = *pair;
            }
        }

        set->source_count = static_cast<std::uint32_t>(sources);
        set->microphone_count = static_cast<std::uint32_t>(receivers - 1);
        keys_ = keys;
        sample_rate_ = sample_rate;
        return set;
    }

    void EnvironmentBuilder::invalidate() noexcept
    {
        for (auto& row : keys_)
        {
            for (Key& key : row)
            {
                key.valid = false;
            }
        }
    }
} // namespace SushiDSP::Room
