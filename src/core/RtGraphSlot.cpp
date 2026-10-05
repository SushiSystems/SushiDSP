/****************************************************************/
/* RtGraphSlot.cpp                                              */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/core/RtGraphSlot.hpp"

#include <algorithm>

namespace SushiDSP
{
    RtGraphSlot::~RtGraphSlot() = default;

    void RtGraphSlot::publish(std::unique_ptr<RigGraph> new_graph)
    {
        // active_ is a non-owning view of whichever entry in retired_ is newest; ownership of
        // every graph ever published lives in retired_ from the moment it's inserted here,
        // keyed by the generation it was published at. store()'s previous value needs no
        // explicit handling: the graph it pointed at is already sitting in retired_ from ITS
        // OWN publish() call, and reclaim() is what eventually frees it.
        //
        // Ordering matters here for two reasons that pull in the same direction:
        //
        //  1. Exception safety: retired_.emplace_back() must take ownership of new_graph BEFORE
        //     active_ is ever pointed at it. If emplace_back() throws (e.g. std::bad_alloc
        //     growing the vector), new_graph (a local unique_ptr) unwinds and frees the graph it
        //     owns; if active_ had already been stored to point at that graph, it would be left
        //     dangling and visible to the audio thread.
        //  2. acquire_for_audio_thread()'s invariant (see its comment) requires active_ to be
        //     stored strictly before publish_generation_ observably advances, so that reading
        //     generation before active_ there can never race ahead of a still-in-flight publish.
        //
        // publish() is documented as single-non-RT-thread-only, so there is no concurrent
        // publish() to race with: compute the next generation locally (no atomic RMW needed),
        // move new_graph into retired_ first (ownership secured, exception-safe), then store
        // active_, and only then publish the new generation number.
        const std::uint64_t generation = publish_generation_.load(std::memory_order_relaxed) + 1;
        retired_.emplace_back(generation, std::move(new_graph));
        active_.store(retired_.back().second.get(), std::memory_order_release);
        publish_generation_.store(generation, std::memory_order_release);
    }

    RigGraph* RtGraphSlot::acquire_for_audio_thread() noexcept
    {
        // Read generation BEFORE active_. publish() stores active_ strictly before bumping
        // publish_generation_, so any graph published at generation <= k (the value read here)
        // has already had its active_ store land. Reading active_ second therefore always
        // observes a graph whose generation is >= k, which means reclaim() -- bounded to
        // generation < k -- can never free the graph this call is about to return.
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
