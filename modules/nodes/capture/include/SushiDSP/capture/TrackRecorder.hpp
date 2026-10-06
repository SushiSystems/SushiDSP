/**************************************************************************/
/* TrackRecorder.hpp                                                      */
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
 * @file TrackRecorder.hpp
 * @brief Declares one recording destination fed by rings and drained by a writer thread.
 * @author Mustafa Garip
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "SushiDSP/core/SpscRing.hpp"
#include "SushiDSP/io/WavWriter.hpp"

namespace SushiDSP
{
    /// @brief One recording destination: the audio thread pushes blocks into one SpscRing<float>
    /// per channel and a writer thread drains all of them in step into a WavWriter. A disk that
    /// cannot keep up costs whole blocks, counted by dropped_blocks(), never a wait in write().
    /// @see docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md §5
    class TrackRecorder
    {
    public:
        /// @brief Sizes the rings and the writer thread's scratch buffers.
        /// @param channels Number of channels to record.
        /// @param sample_rate The file's sample rate in Hz.
        /// @param ring_capacity_frames Frames each channel's ring holds before blocks are dropped.
        TrackRecorder(std::uint32_t channels, double sample_rate, std::size_t ring_capacity_frames);

        /// @brief Calls stop().
        ~TrackRecorder();

        TrackRecorder(const TrackRecorder&) = delete;
        TrackRecorder& operator=(const TrackRecorder&) = delete;

        /// @brief Opens the file, empties the rings and starts the writer thread. Control thread only.
        /// @param path Filesystem path to create or overwrite.
        /// @return true if the file opened and the thread started, false otherwise.
        bool arm(const std::string& path);

        /// @brief Refuses every further block and waits until the audio thread has left write().
        /// Control thread only; it blocks, so it never runs on the audio thread. Calling it twice
        /// is harmless.
        void disarm();

        /// @brief Reports whether the rings hold a whole block, without queueing anything.
        /// Wait-free: two atomic loads and a comparison.
        /// @param frames Number of frames the caller is about to write.
        /// @return true if a write() of that many frames would be queued rather than dropped.
        bool has_room(std::uint32_t frames) const noexcept;

        /// @brief Copies one block into the rings. Wait-free: allocates nothing, takes no lock,
        /// throws nothing, touches no file.
        /// @param channels One pointer per recorded channel, each with at least `frames` samples.
        /// @param frames Number of frames in the block.
        /// @return true if the whole block was queued, false if it was dropped or nothing is armed.
        bool write(const float* const* channels, std::uint32_t frames) noexcept;

        /// @brief Disarms, drains every frame the last accepted block left in the rings, stops
        /// the writer thread and closes the file. Control thread only.
        void stop();

        /// @brief Returns true between a successful arm() and stop().
        bool is_recording() const { return recording_.load(std::memory_order_acquire); }

        /// @brief Returns the frames the writer thread has appended to the file.
        std::uint64_t frames_written() const { return frames_written_.load(std::memory_order_relaxed); }

        /// @brief Returns true when the writer thread gave up because the file could not be written.
        bool failed() const { return failed_.load(std::memory_order_acquire); }

        /// @brief Returns the blocks write() dropped because the rings were full.
        std::uint64_t dropped_blocks() const { return dropped_blocks_.load(std::memory_order_relaxed); }

        /// @brief Returns the per-channel ring capacity in frames.
        std::size_t ring_capacity_frames() const { return ring_capacity_frames_; }

        /// @brief Returns the number of channels recorded.
        std::uint32_t channels() const { return channels_; }

    private:
        /// @brief Copies one block into the rings, with the in-flight count already raised.
        /// @param channels One pointer per recorded channel, each with at least `frames` samples.
        /// @param frames Number of frames in the block.
        /// @return true if the whole block was queued, false if it was dropped or nothing is armed.
        bool write_locked(const float* const* channels, std::uint32_t frames) noexcept;

        /// @brief Blocks the calling thread until no audio-thread call is inside write().
        void wait_for_writers();

        /// @brief Drains the rings until stop() ends the drain and the rings are empty.
        void drain_loop();

        /// @brief Drains up to one scratch buffer of frames from every ring and appends them.
        /// @return The number of frames appended.
        std::size_t drain_once();

        /// @brief Holds one lock-free ring per recorded channel.
        std::vector<std::unique_ptr<SpscRing<float>>> rings_;

        /// @brief Holds the writer thread's per-channel drain buffer.
        std::vector<std::vector<float>>               scratch_;

        /// @brief Points at each scratch buffer, in the shape WavWriter::append() takes.
        std::vector<const float*>                     scratch_pointers_;

        /// @brief Owns the open file between arm() and stop().
        std::unique_ptr<WavWriter>                    writer_;

        /// @brief Runs drain_loop() while the recorder is armed.
        std::thread                                   writer_thread_;

        /// @brief Counts the frames write() has pushed into every ring.
        std::atomic<std::uint64_t> pushed_frames_{ 0 };

        /// @brief Counts the frames the writer thread has popped from every ring.
        std::atomic<std::uint64_t> popped_frames_{ 0 };

        /// @brief Counts the frames the writer thread has appended to the file.
        std::atomic<std::uint64_t> frames_written_{ 0 };

        /// @brief Counts the blocks write() refused because the rings were full.
        std::atomic<std::uint64_t> dropped_blocks_{ 0 };

        /// @brief Marks the span between a successful arm() and stop().
        std::atomic<bool>          recording_{ false };

        /// @brief Counts the audio-thread calls currently inside write().
        std::atomic<std::uint32_t> writers_in_flight_{ 0 };

        /// @brief Keeps the writer thread draining until stop() has seen the last block.
        std::atomic<bool>          draining_{ false };

        /// @brief Marks that an append() threw and the file can take nothing more.
        std::atomic<bool>          failed_{ false };

        /// @brief Holds the file's sample rate in Hz.
        double        sample_rate_;

        /// @brief Holds the frames each channel's ring accepts before blocks are dropped.
        std::size_t   ring_capacity_frames_;

        /// @brief Holds the number of channels recorded.
        std::uint32_t channels_;
    };
}
