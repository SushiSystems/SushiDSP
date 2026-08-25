// BufferPool.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
