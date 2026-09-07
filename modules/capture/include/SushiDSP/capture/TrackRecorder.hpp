/**************************************************************************/
/* TrackRecorder.hpp                                                      */
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

        /// @brief Copies one block into the rings. Wait-free: allocates nothing, takes no lock,
        /// throws nothing, touches no file.
        /// @param channels One pointer per recorded channel, each with at least `frames` samples.
        /// @param frames Number of frames in the block.
        /// @return true if the whole block was queued, false if it was dropped or nothing is armed.
        bool write(const float* const* channels, std::uint32_t frames) noexcept;

        /// @brief Stops the writer thread, drains what is still queued and closes the file.
        void stop();

        /// @brief Returns true between a successful arm() and stop().
        bool is_recording() const { return recording_.load(std::memory_order_acquire); }

        /// @brief Returns the frames the writer thread has appended to the file.
        std::uint64_t frames_written() const { return frames_written_.load(std::memory_order_relaxed); }

        /// @brief Returns the blocks write() dropped because the rings were full.
        std::uint64_t dropped_blocks() const { return dropped_blocks_.load(std::memory_order_relaxed); }

        /// @brief Returns the per-channel ring capacity in frames.
        std::size_t ring_capacity_frames() const { return ring_capacity_frames_; }

        /// @brief Returns the number of channels recorded.
        std::uint32_t channels() const { return channels_; }

    private:
        /// @brief Drains the rings until stop() is called and the rings are empty.
        void drain_loop();

        /// @brief Drains up to one scratch buffer of frames from every ring and appends them.
        /// @return The number of frames appended.
        std::size_t drain_once();

        std::vector<std::unique_ptr<SpscRing<float>>> rings_;
        std::vector<std::vector<float>>               scratch_;
        std::vector<const float*>                     scratch_pointers_;
        std::unique_ptr<WavWriter>                    writer_;
        std::thread                                   writer_thread_;

        std::atomic<std::uint64_t> pushed_frames_{ 0 };
        std::atomic<std::uint64_t> popped_frames_{ 0 };
        std::atomic<std::uint64_t> frames_written_{ 0 };
        std::atomic<std::uint64_t> dropped_blocks_{ 0 };
        std::atomic<bool>          recording_{ false };

        double        sample_rate_;
        std::size_t   ring_capacity_frames_;
        std::uint32_t channels_;
    };
}
