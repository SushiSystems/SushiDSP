/**************************************************************************/
/* track_recorder.cpp                                                     */
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
 * @file track_recorder.cpp
 * @brief Implements the recorder's ring writes and its writer thread.
 * @author Mustafa Garip
 */

#include <SushiDSP/nodes/capture/track_recorder.hpp>

#include <algorithm>
#include <chrono>
#include <exception>

#include <SushiDSP/nodes/capture/capture_log_category.hpp>
#include <SushiDSP/foundation/log/log.hpp>

namespace SushiDSP
{
    namespace
    {
        /// @brief Frames the writer thread moves from the rings to the file in one append.
        constexpr std::size_t K_DRAIN_BLOCK_FRAMES = 4096;

        /// @brief How long the writer thread waits when it finds the rings empty.
        constexpr std::chrono::milliseconds K_IDLE_WAIT{1};
    } // namespace

    TrackRecorder::TrackRecorder(std::uint32_t channels, double sample_rate,
                                 std::size_t ring_capacity_frames)
        : sample_rate_(sample_rate),
          ring_capacity_frames_(std::max<std::size_t>(ring_capacity_frames, 1u)),
          channels_(std::max<std::uint32_t>(channels, 1u))
    {
        const std::size_t scratch_frames = std::min(K_DRAIN_BLOCK_FRAMES, ring_capacity_frames_);

        rings_.reserve(channels_);
        scratch_.reserve(channels_);
        scratch_pointers_.reserve(channels_);

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            rings_.push_back(
                std::unique_ptr<SpscRing<float>>(new SpscRing<float>(ring_capacity_frames_)));
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
        catch (const std::exception& error)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "arm('{}') could not open the take file: {}; the recorder stays disarmed",
                         path, error.what());
            writer_.reset();
            return false;
        }
        catch (...)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "arm('{}') could not open the take file: an exception of unknown type; "
                         "the recorder stays disarmed",
                         path);
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

        const auto disarm_without_writer_thread = [this]
        {
            recording_.store(false, std::memory_order_release);
            draining_.store(false, std::memory_order_release);
            writer_->close();
            writer_.reset();
        };

        try
        {
            writer_thread_ = std::thread(&TrackRecorder::drain_loop, this);
        }
        catch (const std::exception& error)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "arm('{}') could not start the writer thread: {}; the take file is closed "
                         "and the recorder stays disarmed",
                         path, error.what());
            disarm_without_writer_thread();
            return false;
        }
        catch (...)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "arm('{}') could not start the writer thread: an exception of unknown "
                         "type; the take file is closed and the recorder stays disarmed",
                         path);
            disarm_without_writer_thread();
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
                std::this_thread::sleep_for(K_IDLE_WAIT);
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
        const std::size_t available = static_cast<std::size_t>(pushed - popped);
        const std::size_t frames = std::min(available, scratch_[0].size());

        if (frames == 0)
        {
            return 0;
        }

        for (std::uint32_t channel = 0; channel < channels_; ++channel)
        {
            rings_[channel]->pop(scratch_[channel].data(), frames);
        }
        popped_frames_.store(popped + frames, std::memory_order_release);

        const auto stop_after_write_failure = [this]
        {
            failed_.store(true, std::memory_order_release);
            recording_.store(false, std::memory_order_release);
        };

        try
        {
            writer_->append(scratch_pointers_.data(), static_cast<std::uint32_t>(frames));
            frames_written_.store(writer_->frames_written(), std::memory_order_relaxed);
        }
        catch (const std::exception& error)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "appending {} frames to the take file failed after {} frames written: {}; "
                         "recording stops and the frames still queued are discarded",
                         frames, frames_written_.load(std::memory_order_relaxed), error.what());
            stop_after_write_failure();
        }
        catch (...)
        {
            SD_LOG_ERROR(K_CAPTURE_LOG_CATEGORY,
                         "appending {} frames to the take file failed after {} frames written: an "
                         "exception of unknown type; recording stops and the frames still queued "
                         "are discarded",
                         frames, frames_written_.load(std::memory_order_relaxed));
            stop_after_write_failure();
        }

        return frames;
    }
} // namespace SushiDSP
