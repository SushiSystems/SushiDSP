/****************************************************************/
/* TrackRecorder.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file TrackRecorder.cpp
 * @brief Implements the recorder's ring writes and its writer thread.
 * @author Mustafa Garip
 */

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
        // No producer may be inside push() when the rings are cleared, and none may enter after.
        wait_for_writers();

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
            rings_[channel]->clear();
        }
        pushed_frames_.store(0, std::memory_order_relaxed);
        popped_frames_.store(0, std::memory_order_relaxed);
        frames_written_.store(0, std::memory_order_relaxed);
        dropped_blocks_.store(0, std::memory_order_relaxed);
        failed_.store(false, std::memory_order_relaxed);
        draining_.store(true, std::memory_order_relaxed);
        recording_.store(true, std::memory_order_release);

        try
        {
            writer_thread_ = std::thread(&TrackRecorder::drain_loop, this);
        }
        catch (...)
        {
            recording_.store(false, std::memory_order_release);
            draining_.store(false, std::memory_order_release);
            writer_->close();
            writer_.reset();
            return false;
        }

        return true;
    }

    bool TrackRecorder::has_room(std::uint32_t frames) const noexcept
    {
        if (!recording_.load(std::memory_order_acquire))
        {
            return false;
        }

        const std::uint64_t pushed = pushed_frames_.load(std::memory_order_relaxed);
        const std::uint64_t popped = popped_frames_.load(std::memory_order_acquire);
        const std::uint64_t free_frames = ring_capacity_frames_ - (pushed - popped);
        return frames <= free_frames;
    }

    bool TrackRecorder::write(const float* const* channels, std::uint32_t frames) noexcept
    {
        // The in-flight count rises before recording_ is read; see modules/nodes/capture/README.md.
        writers_in_flight_.fetch_add(1, std::memory_order_acquire);

        const bool queued = write_locked(channels, frames);

        writers_in_flight_.fetch_sub(1, std::memory_order_release);
        return queued;
    }

    bool TrackRecorder::write_locked(const float* const* channels, std::uint32_t frames) noexcept
    {
        if (!recording_.load(std::memory_order_acquire) || channels == nullptr)
        {
            return false;
        }
        if (frames == 0)
        {
            return true;
        }

        if (!has_room(frames))
        {
            dropped_blocks_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        const std::uint64_t pushed = pushed_frames_.load(std::memory_order_relaxed);

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            // The free-space check above guarantees the whole run fits, so SpscRing never refuses it.
            rings_[channel]->push(channels[channel], frames);
        }

        pushed_frames_.store(pushed + frames, std::memory_order_release);
        return true;
    }

    void TrackRecorder::wait_for_writers()
    {
        while (writers_in_flight_.load(std::memory_order_acquire) != 0)
        {
            std::this_thread::yield();
        }
    }

    void TrackRecorder::disarm()
    {
        recording_.store(false, std::memory_order_release);
        wait_for_writers();
    }

    void TrackRecorder::stop()
    {
        const bool was_armed = writer_ != nullptr;

        disarm();
        // Every accepted block has landed in the rings by now, so the writer thread's last pass sees all of them.
        draining_.store(false, std::memory_order_release);

        if (writer_thread_.joinable())
        {
            writer_thread_.join();
        }
        if (was_armed && writer_ != nullptr)
        {
            writer_->close();
        }
        writer_.reset();
        failed_.store(false, std::memory_order_release);
    }

    void TrackRecorder::drain_loop()
    {
        while (draining_.load(std::memory_order_acquire))
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
        if (failed_.load(std::memory_order_acquire))
        {
            return 0;
        }

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
            rings_[channel]->pop(scratch_[channel].data(), frames);
        }
        popped_frames_.store(popped + frames, std::memory_order_release);

        try
        {
            writer_->append(scratch_pointers_.data(), static_cast<std::uint32_t>(frames));
            frames_written_.store(writer_->frames_written(), std::memory_order_relaxed);
        }
        catch (...)
        {
            failed_.store(true, std::memory_order_release);
            recording_.store(false, std::memory_order_release);
        }

        return frames;
    }
}
