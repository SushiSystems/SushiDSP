/**************************************************************************/
/* buffer_pool.cpp                                                        */
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
 * @file buffer_pool.cpp
 * @brief Implements the fixed-capacity pool of block-sized float buffers.
 * @author Mustafa Garip
 */

#include "SushiDSP/core/buffer_pool.hpp"

namespace SushiDSP
{
    BufferPool::BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size),
          storage_(static_cast<std::size_t>(block_size) * max_live_buffers, 0.0f),
          slot_live_(max_live_buffers, false)
    {
    }

    float* BufferPool::acquire(std::uint32_t frames)
    {
        if (frames > block_size_)
        {
            return nullptr;
        }

        for (std::size_t slot = 0; slot < slot_live_.size(); ++slot)
        {
            if (!slot_live_[slot])
            {
                slot_live_[slot] = true;
                return storage_.data() + slot * block_size_;
            }
        }

        return nullptr;
    }

    void BufferPool::release(float* buffer)
    {
        const std::size_t offset = static_cast<std::size_t>(buffer - storage_.data());
        const std::size_t slot = offset / block_size_;
        slot_live_[slot] = false;
    }

    std::uint32_t BufferPool::capacity() const
    {
        return static_cast<std::uint32_t>(slot_live_.size());
    }
} // namespace SushiDSP
