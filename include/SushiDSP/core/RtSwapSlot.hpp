/**************************************************************************/
/* RtSwapSlot.hpp                                                         */
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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace SushiDSP
{
    /// @brief Publishes replacement objects of type T to the audio thread without ever locking
    /// or blocking it. A non-RT thread builds a whole new T and publish()es it; the audio thread
    /// reads the active object with one atomic load per callback. Retired objects are freed only
    /// once the audio thread has provably moved past them, checked by reclaim() on the non-RT
    /// thread.
    ///
    /// Threading contract:
    ///  - publish() and reclaim() must both be called from a single, consistent non-RT thread.
    ///    If callers ever need to call either from more than one thread, they must externally
    ///    synchronize those calls themselves - this class provides no such synchronization on
    ///    its own.
    ///  - acquire_for_audio_thread() must be called from a single, consistent RT (audio)
    ///    thread. The pointer it returns is only valid until that SAME thread's NEXT call to
    ///    acquire_for_audio_thread(); it must not be retained across calls (a subsequent
    ///    reclaim() may free the object it pointed at once this thread has moved past it).
    ///
    /// @tparam T The published object type. Owned by the slot; destroyed by reclaim() or by the
    /// slot's destructor.
    template <typename T>
    class RtSwapSlot
    {
    public:
        RtSwapSlot() = default;
        ~RtSwapSlot() = default;

        RtSwapSlot(const RtSwapSlot&) = delete;
        RtSwapSlot& operator=(const RtSwapSlot&) = delete;

        /// @brief Makes new_object the active object for subsequent acquire_for_audio_thread()
        /// calls. Not real-time safe; call only off the audio thread. Must be called from a
        /// single, consistent non-RT thread (or externally synchronized if called from more
        /// than one) - see the class-level threading contract.
        /// @param new_object The fully constructed object to publish. Must not be null.
        void publish(std::unique_ptr<T> new_object)
        {
            // active_ is a non-owning view of whichever entry in retired_ is newest; ownership of
            // every object ever published lives in retired_ from the moment it is inserted here,
            // keyed by the generation it was published at. store()'s previous value needs no
            // explicit handling: the object it pointed at is already sitting in retired_ from ITS
            // OWN publish() call, and reclaim() is what eventually frees it.
            //
            // Ordering matters here for two reasons that pull in the same direction:
            //
            //  1. Exception safety: retired_.emplace_back() must take ownership of new_object
            //     BEFORE active_ is ever pointed at it. If emplace_back() throws (e.g.
            //     std::bad_alloc growing the vector), new_object (a local unique_ptr) unwinds and
            //     frees the object it owns; if active_ had already been stored to point at that
            //     object, it would be left dangling and visible to the audio thread.
            //  2. acquire_for_audio_thread()'s invariant (see its comment) requires active_ to be
            //     stored strictly before publish_generation_ observably advances, so that reading
            //     generation before active_ there can never race ahead of a still-in-flight
            //     publish.
            //
            // publish() is documented as single-non-RT-thread-only, so there is no concurrent
            // publish() to race with: compute the next generation locally (no atomic RMW needed),
            // move new_object into retired_ first (ownership secured, exception-safe), then store
            // active_, and only then publish the new generation number.
            const std::uint64_t generation = publish_generation_.load(std::memory_order_relaxed) + 1;
            retired_.emplace_back(generation, std::move(new_object));
            active_.store(retired_.back().second.get(), std::memory_order_release);
            publish_generation_.store(generation, std::memory_order_release);
        }

        /// @brief Returns the current active object and records that this generation is now in
        /// use. Real-time safe: one atomic load, one atomic store, no allocation. Must be
        /// called from a single, consistent audio thread - see the class-level threading
        /// contract.
        /// @return The active object, or nullptr if publish() was never called. The returned
        /// pointer is valid only until this same thread's NEXT call to
        /// acquire_for_audio_thread(); do not retain it across calls.
        T* acquire_for_audio_thread() noexcept
        {
            // Read generation BEFORE active_. publish() stores active_ strictly before bumping
            // publish_generation_, so any object published at generation <= k (the value read
            // here) has already had its active_ store land. Reading active_ second therefore
            // always observes an object whose generation is >= k, which means reclaim() --
            // bounded to generation < k -- can never free the object this call is about to
            // return.
            const std::uint64_t generation = publish_generation_.load(std::memory_order_acquire);
            audio_thread_generation_.store(generation, std::memory_order_release);
            return active_.load(std::memory_order_acquire);
        }

        /// @brief Reports whether anything has been published yet. Real-time safe: one atomic
        /// load, no allocation. Unlike acquire_for_audio_thread() this records no generation, so
        /// it neither holds a retired object alive nor releases one.
        /// @return True if an active object exists, false if publish() was never called.
        bool has_active() const noexcept
        {
            return active_.load(std::memory_order_acquire) != nullptr;
        }

        /// @brief Returns the active object without recording a generation. Publisher-thread
        /// only: call it from the same non-RT thread as publish() and reclaim(). That thread owns
        /// retired_, so nothing can free the returned object underneath it. Unlike
        /// acquire_for_audio_thread() it records nothing, so it can neither hold a retired object
        /// alive nor - the reason it exists - forge the audio thread's progress watermark and let
        /// a later reclaim() free an object the audio thread is still using.
        /// @return The active object, or nullptr if publish() was never called.
        T* active_for_publisher() const noexcept
        {
            return active_.load(std::memory_order_acquire);
        }

        /// @brief Frees every retired object the audio thread has moved past. Not real-time
        /// safe; call periodically off the audio thread (e.g. once per GUI frame). Must be
        /// called from the same single, consistent non-RT thread as publish() (or externally
        /// synchronized if called from more than one) - see the class-level threading contract.
        void reclaim()
        {
            const std::uint64_t safe_generation = audio_thread_generation_.load(std::memory_order_acquire);
            // A zero watermark means acquire_for_audio_thread() has never returned a non-null
            // object, so no retired entry can be held by the audio thread and every one but the
            // active object is past.
            const bool nothing_acquired = safe_generation == 0;
            retired_.erase(
                std::remove_if(retired_.begin(), retired_.end(),
                    [&](const std::pair<std::uint64_t, std::unique_ptr<T>>& entry)
                    {
                        const bool past = nothing_acquired || entry.first < safe_generation;
                        return past && entry.second.get() != active_.load(std::memory_order_acquire);
                    }),
                retired_.end());
        }

    private:
        std::atomic<T*>            active_{ nullptr };
        std::atomic<std::uint64_t> audio_thread_generation_{ 0 };
        std::atomic<std::uint64_t> publish_generation_{ 0 };
        std::vector<std::pair<std::uint64_t, std::unique_ptr<T>>> retired_;
    };
}
