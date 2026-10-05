/****************************************************************/
/* BufferPool.cpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/core/BufferPool.hpp"

namespace SushiDSP
{
    BufferPool::BufferPool(std::uint32_t block_size, std::uint32_t max_live_buffers)
        : block_size_(block_size)
        , storage_(static_cast<std::size_t>(block_size) * max_live_buffers, 0.0f)
        , slot_live_(max_live_buffers, false)
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
        const std::size_t slot   = offset / block_size_;
        slot_live_[slot] = false;
    }

    std::uint32_t BufferPool::capacity() const
    {
        return static_cast<std::uint32_t>(slot_live_.size());
    }
}
