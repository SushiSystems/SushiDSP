/**************************************************************************/
/* SpscRing.hpp                                                           */
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
        std::size_t advance(std::size_t index) const
        {
            return (index + 1) % storage_.size();
        }

        std::vector<T>            storage_;
        std::atomic<std::size_t>  head_;
        std::atomic<std::size_t>  tail_;
    };
}
