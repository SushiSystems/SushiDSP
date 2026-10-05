/****************************************************************/
/* RtGraphSlot.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RtGraphSlot.cpp
 * @brief Implements the slot that publishes RigGraph replacements to the audio thread.
 * @author Mustafa Garip
 */

#include "SushiDSP/core/RtGraphSlot.hpp"

#include <algorithm>

namespace SushiDSP
{
    RtGraphSlot::~RtGraphSlot() = default;

    void RtGraphSlot::publish(std::unique_ptr<RigGraph> new_graph)
    {
        // Order: retired_ owns it, then active_, then the generation (include/SushiDSP/README.md).
        const std::uint64_t generation = publish_generation_.load(std::memory_order_relaxed) + 1;
        retired_.emplace_back(generation, std::move(new_graph));
        active_.store(retired_.back().second.get(), std::memory_order_release);
        publish_generation_.store(generation, std::memory_order_release);
    }

    RigGraph* RtGraphSlot::acquire_for_audio_thread() noexcept
    {
        // The generation is read before active_ (include/SushiDSP/README.md).
        const std::uint64_t generation = publish_generation_.load(std::memory_order_acquire);
        audio_thread_generation_.store(generation, std::memory_order_release);
        return active_.load(std::memory_order_acquire);
    }

    void RtGraphSlot::reclaim()
    {
        const std::uint64_t safe_generation = audio_thread_generation_.load(std::memory_order_acquire);
        retired_.erase(
            std::remove_if(retired_.begin(), retired_.end(),
                [&](const std::pair<std::uint64_t, std::unique_ptr<RigGraph>>& entry)
                {
                    return entry.first < safe_generation && entry.second.get() != active_.load(std::memory_order_acquire);
                }),
            retired_.end());
    }
}
