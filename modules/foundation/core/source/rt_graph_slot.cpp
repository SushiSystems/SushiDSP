/**************************************************************************/
/* rt_graph_slot.cpp                                                      */
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
 * @file rt_graph_slot.cpp
 * @brief Implements the slot that publishes RigGraph replacements to the audio thread.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/core/rt_graph_slot.hpp>

#include <algorithm>

namespace SushiDSP
{
    RtGraphSlot::~RtGraphSlot() = default;

    void RtGraphSlot::publish(std::unique_ptr<RigGraph> new_graph)
    {
        // Order: retired_ owns it, then active_, then the generation (modules/foundation/core/README.md).
        const std::uint64_t generation = publish_generation_.load(std::memory_order_relaxed) + 1;
        retired_.emplace_back(generation, std::move(new_graph));
        active_.store(retired_.back().second.get(), std::memory_order_release);
        publish_generation_.store(generation, std::memory_order_release);
    }

    RigGraph* RtGraphSlot::acquire_for_audio_thread() noexcept
    {
        // The generation is read before active_ (modules/foundation/core/README.md).
        const std::uint64_t generation = publish_generation_.load(std::memory_order_acquire);
        audio_thread_generation_.store(generation, std::memory_order_release);
        return active_.load(std::memory_order_acquire);
    }

    void RtGraphSlot::reclaim()
    {
        const std::uint64_t safe_generation =
            audio_thread_generation_.load(std::memory_order_acquire);
        retired_.erase(
            std::remove_if(retired_.begin(), retired_.end(),
                           [&](const std::pair<std::uint64_t, std::unique_ptr<RigGraph>>& entry)
                           {
                               return entry.first < safe_generation &&
                                      entry.second.get() != active_.load(std::memory_order_acquire);
                           }),
            retired_.end());
    }
} // namespace SushiDSP
