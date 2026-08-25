/**************************************************************************/
/* BufferPool.hpp                                                         */
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

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Fixed-capacity pool of block-sized float buffers with allocation-free acquire/release.
    class BufferPool
    {
    public:
        /// @brief Preallocates max_live_buffers buffers of block_size floats each.
        /// @param block_size Number of floats per buffer.
        /// @param max_live_buffers Maximum number of buffers live at once.
        BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers);

        /// @brief Returns a buffer of at least frames floats, or nullptr if the pool is exhausted.
        /// @param frames Number of frames the caller needs.
        /// @return Pointer to a block_size-float buffer, or nullptr.
        float* acquire(std::uint32_t frames);

        /// @brief Returns a buffer previously obtained from acquire() to the pool.
        /// @param buffer Pointer previously returned by acquire().
        void release(float* buffer);

        /// @brief The maximum number of buffers that can be live at once.
        /// @return The pool capacity in buffers.
        std::uint32_t capacity() const;

    private:
        std::uint32_t         block_size_;
        std::vector<float>    storage_;
        std::vector<bool>     slot_live_;
    };
}
