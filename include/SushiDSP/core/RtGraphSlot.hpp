/**************************************************************************/
/* RtGraphSlot.hpp                                                        */
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

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "SushiDSP/core/RigGraph.hpp"

namespace SushiDSP
{
    /// @brief Publishes RigGraph replacements to the audio thread without ever locking or
    /// blocking it. A non-RT thread builds a whole new compiled RigGraph and publish()es it;
    /// the audio thread reads the active graph with one atomic load per callback. Retired
    /// graphs are freed only once the audio thread has provably moved past them, checked by
    /// reclaim() on the non-RT thread.
    class RtGraphSlot
    {
    public:
        RtGraphSlot() = default;
        ~RtGraphSlot();

        RtGraphSlot(const RtGraphSlot&) = delete;
        RtGraphSlot& operator=(const RtGraphSlot&) = delete;

        /// @brief Makes new_graph the active graph for subsequent acquire_for_audio_thread()
        /// calls. Not real-time safe; call only off the audio thread.
        /// @param new_graph The fully compiled graph to publish. Must not be null.
        void publish(std::unique_ptr<RigGraph> new_graph);

        /// @brief Returns the current active graph and records that this generation is now in
        /// use. Real-time safe: one atomic load, one atomic store, no allocation.
        /// @return The active graph, or nullptr if publish() was never called.
        RigGraph* acquire_for_audio_thread();

        /// @brief Frees every retired graph the audio thread has moved past. Not real-time
        /// safe; call periodically off the audio thread (e.g. once per GUI frame).
        void reclaim();

    private:
        std::atomic<RigGraph*>       active_{ nullptr };
        std::atomic<std::uint64_t>   audio_thread_generation_{ 0 };
        std::atomic<std::uint64_t>   publish_generation_{ 0 };
        std::vector<std::pair<std::uint64_t, std::unique_ptr<RigGraph>>> retired_;
    };
}
