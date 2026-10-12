/**************************************************************************/
/* rt_graph_slot.hpp                                                      */
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
 * @file rt_graph_slot.hpp
 * @brief Declares the slot that publishes RigGraph replacements to the audio thread.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "SushiDSP/core/rig_graph.hpp"

namespace SushiDSP
{
    /// @brief Publishes RigGraph replacements to the audio thread without locking or blocking
    /// it, and frees a retired graph only once the audio thread has moved past it.
    ///
    /// publish() and reclaim() belong to one non-RT thread and acquire_for_audio_thread() to
    /// one audio thread; the slot synchronises neither side against itself
    /// (modules/foundation/core/README.md).
    class RtGraphSlot
    {
        public:
            RtGraphSlot() = default;
            ~RtGraphSlot();

            RtGraphSlot(const RtGraphSlot&) = delete;
            RtGraphSlot& operator=(const RtGraphSlot&) = delete;

            /// @brief Makes new_graph the active graph for subsequent acquire_for_audio_thread()
            /// calls. Not real-time safe; call only off the audio thread. Must be called from a
            /// single, consistent non-RT thread (or externally synchronized if called from more
            /// than one) - see the class-level threading contract.
            /// @param new_graph The fully compiled graph to publish. Must not be null.
            void publish(std::unique_ptr<RigGraph> new_graph);

            /// @brief Returns the current active graph and records that this generation is now in
            /// use. Real-time safe: one atomic load, one atomic store, no allocation. Must be
            /// called from a single, consistent audio thread - see the class-level threading
            /// contract.
            /// @return The active graph, or nullptr if publish() was never called. The returned
            /// pointer is valid only until this same thread's NEXT call to
            /// acquire_for_audio_thread(); do not retain it across calls.
            RigGraph* acquire_for_audio_thread() noexcept;

            /// @brief Frees every retired graph the audio thread has moved past. Not real-time
            /// safe; call periodically off the audio thread (e.g. once per GUI frame). Must be
            /// called from the same single, consistent non-RT thread as publish() (or externally
            /// synchronized if called from more than one) - see the class-level threading contract.
            void reclaim();

        private:
            std::atomic<RigGraph*> active_{nullptr};
            std::atomic<std::uint64_t> audio_thread_generation_{0};
            std::atomic<std::uint64_t> publish_generation_{0};
            std::vector<std::pair<std::uint64_t, std::unique_ptr<RigGraph>>> retired_;
    };
} // namespace SushiDSP
