/**************************************************************************/
/* SpscRing.hpp                                                           */
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
 * @file SpscRing.hpp
 * @brief Defines a lock-free single-producer, single-consumer ring buffer.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <vector>

namespace SushiDSP
{
    /// @brief Lock-free single-producer/single-consumer ring buffer. push() runs on the control
    /// thread, pop() runs on the audio thread; neither blocks nor allocates after construction.
    template<typename T>
    class SpscRing
    {
    public:
        /// @brief Preallocates storage for capacity elements.
        /// @param capacity Maximum number of elements the ring can hold at once.
        explicit SpscRing(std::size_t capacity)
            : storage_(capacity + 1)
            , head_(0)
            , tail_(0)
        {
        }

        /// @brief Attempts to enqueue a value. Producer-thread only.
        /// @param value The value to enqueue.
        /// @return true if enqueued, false if the ring is full.
        bool push(const T& value)
        {
            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t next_tail = advance(current_tail);

            if (next_tail == head_.load(std::memory_order_acquire))
            {
                return false;
            }

            storage_[current_tail] = value;
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        /// @brief Attempts to enqueue a whole run of values, all of them or none of them.
        /// Producer-thread only. Wait-free: one acquire load, one release store and two copies.
        /// @param values The values to enqueue.
        /// @param count How many values to enqueue.
        /// @return true if every value was enqueued, false if the ring holds too little room.
        bool push(const T* values, std::size_t count)
        {
            if (count == 0)
            {
                return true;
            }

            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t current_head = head_.load(std::memory_order_acquire);
            const std::size_t used = current_tail >= current_head
                ? current_tail - current_head
                : current_tail + storage_.size() - current_head;
            if (count > capacity() - used)
            {
                return false;
            }

            const std::size_t first = std::min(count, storage_.size() - current_tail);
            std::copy(values, values + first, storage_.begin() + static_cast<std::ptrdiff_t>(current_tail));
            std::copy(values + first, values + count, storage_.begin());
            tail_.store(wrap(current_tail + count), std::memory_order_release);
            return true;
        }

        /// @brief Attempts to dequeue a whole run of values, all of them or none of them.
        /// Consumer-thread only. Wait-free: one acquire load, one release store and two copies.
        /// @param out Receives the dequeued values; at least count elements long.
        /// @param count How many values to dequeue.
        /// @return true if every value was dequeued, false if the ring holds fewer than count.
        bool pop(T* out, std::size_t count)
        {
            if (count == 0)
            {
                return true;
            }

            const std::size_t current_head = head_.load(std::memory_order_relaxed);
            const std::size_t current_tail = tail_.load(std::memory_order_acquire);
            const std::size_t available = current_tail >= current_head
                ? current_tail - current_head
                : current_tail + storage_.size() - current_head;
            if (count > available)
            {
                return false;
            }

            const std::size_t first = std::min(count, storage_.size() - current_head);
            std::copy(storage_.begin() + static_cast<std::ptrdiff_t>(current_head),
                      storage_.begin() + static_cast<std::ptrdiff_t>(current_head + first), out);
            std::copy(storage_.begin(), storage_.begin() + static_cast<std::ptrdiff_t>(count - first),
                      out + first);
            head_.store(wrap(current_head + count), std::memory_order_release);
            return true;
        }

        /// @brief Attempts to dequeue a value. Consumer-thread only.
        /// @param out Receives the dequeued value on success.
        /// @return true if a value was dequeued, false if the ring is empty.
        bool pop(T& out)
        {
            const std::size_t current_head = head_.load(std::memory_order_relaxed);

            if (current_head == tail_.load(std::memory_order_acquire))
            {
                return false;
            }

            out = storage_[current_head];
            head_.store(advance(current_head), std::memory_order_release);
            return true;
        }

        /// @brief Empties the ring in place, keeping the storage the constructor allocated.
        /// @pre Neither the producer nor the consumer is inside push() or pop().
        void clear()
        {
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_release);
        }

        /// @brief The maximum number of elements this ring can hold at once.
        /// @return The capacity passed to the constructor.
        std::size_t capacity() const
        {
            return storage_.size() - 1;
        }

    private:
        /// @brief Returns the index one step past the given one, wrapping at the storage end.
        /// @param index The index to step from.
        /// @return The next index.
        std::size_t advance(std::size_t index) const
        {
            return wrap(index + 1);
        }

        /// @brief Folds an index at most one storage length past the start back into range.
        /// @param index The index to fold.
        /// @return The folded index.
        /// @pre index is less than twice the storage length.
        std::size_t wrap(std::size_t index) const
        {
            return index >= storage_.size() ? index - storage_.size() : index;
        }

        std::vector<T>            storage_;
        std::atomic<std::size_t>  head_;
        std::atomic<std::size_t>  tail_;
    };
}
