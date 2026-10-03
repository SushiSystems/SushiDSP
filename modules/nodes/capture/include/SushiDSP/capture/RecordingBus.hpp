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
    /// stopping disarms both before either closes for the same reason. A block reaches both
    /// recorders or neither, so the two files hold the same frame count under any drop.
    /// @see docs/archive/superpowers/specs/2026-09-07-stereo-tracks-design.md §5
    class RecordingBus
    {
    public:
        /// @brief Frames each ring holds, 1.37 seconds at 48 kHz.
        static constexpr std::size_t kRingCapacityFrames = 65536;

        /// @brief Builds both recorders.
        /// @param processed_channels Number of channels the rig outputs.
        /// @param sample_rate The files' sample rate in Hz.
        /// @param processed_ring_frames Frames the processed recorder's rings hold.
        /// @param dry_ring_frames Frames the dry recorder's ring holds.
        RecordingBus(std::uint32_t processed_channels, double sample_rate,
                     std::size_t processed_ring_frames = kRingCapacityFrames,
                     std::size_t dry_ring_frames = kRingCapacityFrames);

        RecordingBus(const RecordingBus&) = delete;
        RecordingBus& operator=(const RecordingBus&) = delete;

        /// @brief Opens both files. Control thread only.
        /// @param folder Directory the two files are created in.
        /// @param take_name Base name both file names are built from.
        /// @return true if both files opened; on failure neither records.
        bool arm(const std::string& folder, const std::string& take_name);

        /// @brief Queues one block into both files when both recorders have room, into neither
        /// when one of them does not, and advances the block index either way. Wait-free:
        /// allocates nothing, takes no lock, throws nothing, touches no file.
        /// @param processed_channels One pointer per processed channel, the rig's output.
        /// @param dry_channel One pointer to the device input that drove the rig.
        /// @param frames Number of frames in the block, the same for both legs.
        /// @return true if the block was queued; false when the bus dropped it from both files.
        bool write(const float* const* processed_channels, const float* dry_channel,
                   std::uint32_t frames) noexcept;

        /// @brief Disarms both recorders before either stops, so no block can reach one file
        /// after the other has closed, then drains and closes both. Control thread only.
        void stop();

        /// @brief Returns true between a successful arm() and stop().
        bool is_recording() const { return processed_.is_recording() && dry_.is_recording(); }

        /// @brief Returns true when either recorder's writer thread gave up on its file.
        bool failed() const { return processed_.failed() || dry_.failed(); }

        /// @brief Returns how many blocks write() has taken since construction.
        std::uint64_t block_index() const { return block_index_.load(std::memory_order_relaxed); }

        /// @brief Returns the frames written to <take_name>-processed.wav.
        std::uint64_t processed_frames_written() const { return processed_.frames_written(); }

        /// @brief Returns the frames written to <take_name>-dry.wav.
        std::uint64_t dry_frames_written() const { return dry_.frames_written(); }

        /// @brief Returns the blocks the bus kept out of both files.
        std::uint64_t dropped_blocks() const
        {
            return dropped_blocks_.load(std::memory_order_relaxed);
        }

    private:
        /// @brief Records the rig's output into <take_name>-processed.wav.
        TrackRecorder              processed_;

        /// @brief Records the device input into <take_name>-dry.wav.
        TrackRecorder              dry_;

        /// @brief Counts the blocks write() has taken since construction.
        std::atomic<std::uint64_t> block_index_{ 0 };

        /// @brief Counts the blocks the bus refused because one recorder had no room.
        std::atomic<std::uint64_t> dropped_blocks_{ 0 };
    };
}
