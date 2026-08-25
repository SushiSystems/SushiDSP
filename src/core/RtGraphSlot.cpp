/**************************************************************************/
/* RtGraphSlot.cpp                                                        */
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

#include "SushiDSP/core/RtGraphSlot.hpp"

#include <algorithm>

namespace SushiDSP
{
    RtGraphSlot::~RtGraphSlot() = default;

    void RtGraphSlot::publish(std::unique_ptr<RigGraph> new_graph)
    {
        // active_ is a non-owning view of whichever entry in retired_ is newest; ownership of
        // every graph ever published lives in retired_ from the moment it's inserted here,
        // keyed by the generation it was published at. exchange()'s old value needs no
        // explicit handling: its owning unique_ptr is already sitting in retired_ from ITS
        // OWN publish() call, and reclaim() is what eventually frees it.
        active_.store(new_graph.get(), std::memory_order_release);
        const std::uint64_t generation = publish_generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
        retired_.emplace_back(generation, std::move(new_graph));
    }

    RigGraph* RtGraphSlot::acquire_for_audio_thread()
    {
        RigGraph* graph = active_.load(std::memory_order_acquire);
        audio_thread_generation_.store(publish_generation_.load(std::memory_order_acquire), std::memory_order_release);
        return graph;
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
