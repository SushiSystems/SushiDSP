/**************************************************************************/
/* TrackRecorder.cpp                                                      */
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

#include "SushiDSP/capture/TrackRecorder.hpp"

#include <algorithm>
#include <chrono>

namespace SushiDSP
{
    namespace
    {
        /// @brief Frames the writer thread moves from the rings to the file in one append.
        constexpr std::size_t kDrainBlockFrames = 4096;

        /// @brief How long the writer thread waits when it finds the rings empty.
        constexpr std::chrono::milliseconds kIdleWait{ 1 };
    }

    TrackRecorder::TrackRecorder(std::uint32_t channels, double sample_rate,
                                 std::size_t ring_capacity_frames)
        : sample_rate_(sample_rate)
        , ring_capacity_frames_(std::max<std::size_t>(ring_capacity_frames, 1u))
        , channels_(std::max<std::uint32_t>(channels, 1u))
    {
        const std::size_t scratch_frames = std::min(kDrainBlockFrames, ring_capacity_frames_);

        rings_.reserve(channels_);
        scratch_.reserve(channels_);
        scratch_pointers_.reserve(channels_);

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            rings_.push_back(std::unique_ptr<SpscRing<float>>(
                new SpscRing<float>(ring_capacity_frames_)));
            scratch_.emplace_back(scratch_frames, 0.0f);
        }
        for (std::vector<float>& buffer : scratch_)
        {
            scratch_pointers_.push_back(buffer.data());
        }
    }

    TrackRecorder::~TrackRecorder()
    {
        stop();
    }

    bool TrackRecorder::arm(const std::string& path)
    {
        if (recording_.load(std::memory_order_acquire))
        {
            return false;
        }

        try
        {
            writer_.reset(new WavWriter(path, sample_rate_, channels_, WavWriter::Format::Float32));
        }
        catch (...)
        {
            writer_.reset();
            return false;
        }

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            rings_[channel].reset(new SpscRing<float>(ring_capacity_frames_));
        }
        pushed_frames_.store(0, std::memory_order_relaxed);
        popped_frames_.store(0, std::memory_order_relaxed);
        frames_written_.store(0, std::memory_order_relaxed);
        dropped_blocks_.store(0, std::memory_order_relaxed);
        recording_.store(true, std::memory_order_release);

        try
        {
            writer_thread_ = std::thread(&TrackRecorder::drain_loop, this);
        }
        catch (...)
        {
            recording_.store(false, std::memory_order_release);
            writer_->close();
            writer_.reset();
            return false;
        }

        return true;
    }

    bool TrackRecorder::write(const float* const* channels, std::uint32_t frames) noexcept
    {
        if (!recording_.load(std::memory_order_acquire) || channels == nullptr)
        {
            return false;
        }
        if (frames == 0)
        {
            return true;
        }

        const std::uint64_t pushed = pushed_frames_.load(std::memory_order_relaxed);
        const std::uint64_t popped = popped_frames_.load(std::memory_order_acquire);
        const std::uint64_t free_frames = ring_capacity_frames_ - (pushed - popped);

        if (frames > free_frames)
        {
            dropped_blocks_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            const float*      source = channels[channel];
            SpscRing<float>&  ring   = *rings_[channel];
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                ring.push(source[frame]);
            }
        }

        pushed_frames_.store(pushed + frames, std::memory_order_release);
        return true;
    }

    void TrackRecorder::stop()
    {
        const bool was_recording = recording_.exchange(false, std::memory_order_acq_rel);

        if (writer_thread_.joinable())
        {
            writer_thread_.join();
        }
        if (was_recording && writer_ != nullptr)
        {
            writer_->close();
        }
        writer_.reset();
    }

    void TrackRecorder::drain_loop()
    {
        while (recording_.load(std::memory_order_acquire))
        {
            if (drain_once() == 0)
            {
                std::this_thread::sleep_for(kIdleWait);
            }
        }
        while (drain_once() != 0)
        {
        }
    }

    std::size_t TrackRecorder::drain_once()
    {
        const std::uint64_t pushed = pushed_frames_.load(std::memory_order_acquire);
        const std::uint64_t popped = popped_frames_.load(std::memory_order_relaxed);
        const std::size_t   available = static_cast<std::size_t>(pushed - popped);
        const std::size_t   frames = std::min(available, scratch_[0].size());

        if (frames == 0)
        {
            return 0;
        }

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            SpscRing<float>&   ring   = *rings_[channel];
            std::vector<float>& buffer = scratch_[channel];
            for (std::size_t frame = 0; frame < frames; ++frame)
            {
                ring.pop(buffer[frame]);
            }
        }
        popped_frames_.store(popped + frames, std::memory_order_release);

        try
        {
            writer_->append(scratch_pointers_.data(), static_cast<std::uint32_t>(frames));
            frames_written_.store(writer_->frames_written(), std::memory_order_relaxed);
        }
        catch (...)
        {
            recording_.store(false, std::memory_order_release);
        }

        return frames;
    }
}
