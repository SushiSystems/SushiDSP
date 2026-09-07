/**************************************************************************/
/* RecordingBus.hpp                                                       */
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
#include <string>

#include "SushiDSP/capture/TrackRecorder.hpp"

namespace SushiDSP
{
    /// @brief The pair of recorders the host arms together: the rig's output into
    /// <take_name>-processed.wav and the device input into <take_name>-dry.wav. Arming both in one
    /// call is what makes the two files sample-aligned, because they begin on the same block, and
    /// the bus keeps the block index they began at so a later re-amp lines up.
    class RecordingBus
    {
    public:
        /// @brief Frames each ring holds, 1.37 seconds at 48 kHz.
        static constexpr std::size_t kRingCapacityFrames = 65536;

        /// @brief Builds both recorders.
        /// @param processed_channels Number of channels the rig outputs.
        /// @param sample_rate The files' sample rate in Hz.
        RecordingBus(std::uint32_t processed_channels, double sample_rate);

        RecordingBus(const RecordingBus&) = delete;
        RecordingBus& operator=(const RecordingBus&) = delete;

        /// @brief Opens both files and records the block index they began at. Control thread only.
        /// @param folder Directory the two files are created in.
        /// @param take_name Base name both file names are built from.
        /// @return true if both files opened; on failure neither records.
        bool arm(const std::string& folder, const std::string& take_name);

        /// @brief Queues one block of the rig's output and advances the block index.
        /// @param channels One pointer per processed channel.
        /// @param frames Number of frames in the block.
        /// @return true if the block was queued.
        bool write_processed(const float* const* channels, std::uint32_t frames) noexcept;

        /// @brief Queues one block of the device input.
        /// @param channels One pointer to the dry channel.
        /// @param frames Number of frames in the block.
        /// @return true if the block was queued.
        bool write_dry(const float* const* channels, std::uint32_t frames) noexcept;

        /// @brief Stops both recorders and closes both files.
        void stop();

        /// @brief Returns true between a successful arm() and stop().
        bool is_recording() const { return processed_.is_recording() && dry_.is_recording(); }

        /// @brief Returns the block index both files began at.
        std::uint64_t armed_block_index() const { return armed_block_index_; }

        /// @brief Returns how many blocks write_processed() has taken since construction.
        std::uint64_t block_index() const { return block_index_.load(std::memory_order_relaxed); }

        /// @brief Returns the frames written to <take_name>-processed.wav.
        std::uint64_t processed_frames_written() const { return processed_.frames_written(); }

        /// @brief Returns the frames written to <take_name>-dry.wav.
        std::uint64_t dry_frames_written() const { return dry_.frames_written(); }

        /// @brief Returns the blocks dropped by either recorder.
        std::uint64_t dropped_blocks() const
        {
            return processed_.dropped_blocks() + dry_.dropped_blocks();
        }

    private:
        TrackRecorder              processed_;
        TrackRecorder              dry_;
        std::atomic<std::uint64_t> block_index_{ 0 };
        std::uint64_t              armed_block_index_ = 0;
    };
}
