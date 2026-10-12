/**************************************************************************/
/* buffer_pool.hpp                                                        */
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
 * @file buffer_pool.hpp
 * @brief Declares a fixed-capacity pool of block-sized float buffers.
 * @author Mustafa Garip
 */

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

            /// @brief Returns a buffer obtained from acquire() to the pool.
            /// @param buffer A pointer acquire() returned.
            void release(float* buffer);

            /// @brief The maximum number of buffers that can be live at once.
            /// @return The pool capacity in buffers.
            std::uint32_t capacity() const;

        private:
            std::uint32_t block_size_;
            std::vector<float> storage_;
            std::vector<bool> slot_live_;
    };
} // namespace SushiDSP
