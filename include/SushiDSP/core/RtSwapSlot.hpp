/****************************************************************/
/* RtSwapSlot.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RtSwapSlot.hpp
 * @brief Defines the slot that publishes replacement objects to the audio thread.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace SushiDSP
{
    /// @brief Publishes replacement objects of type T to the audio thread without locking or
    /// blocking it, and frees a retired object only once the audio thread has moved past it.
    ///
    /// publish() and reclaim() belong to one non-RT thread and acquire_for_audio_thread() to
    /// one audio thread; the slot synchronises neither side against itself
    /// (include/SushiDSP/README.md).
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
            // Order: retired_ owns it, then active_, then the generation (include/SushiDSP/README.md).
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
            // The generation is read before active_ (include/SushiDSP/README.md).
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
            // A zero watermark means the audio thread holds no retired entry.
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
