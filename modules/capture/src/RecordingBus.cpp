/**************************************************************************/
/* RecordingBus.cpp                                                       */
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

#include "SushiDSP/capture/RecordingBus.hpp"

namespace SushiDSP
{
    namespace
    {
        /// @brief Joins a folder and a file name with a separator when the folder needs one.
        /// @param folder Directory the file lives in; may be empty.
        /// @param file_name Name of the file inside that directory.
        /// @return The joined path.
        std::string join_path(const std::string& folder, const std::string& file_name)
        {
            if (folder.empty())
            {
                return file_name;
            }
            const char last = folder.back();
            if (last == '/' || last == '\\')
            {
                return folder + file_name;
            }
            return folder + '/' + file_name;
        }
    }

    RecordingBus::RecordingBus(std::uint32_t processed_channels, double sample_rate,
                               std::size_t processed_ring_frames, std::size_t dry_ring_frames)
        : processed_(processed_channels, sample_rate, processed_ring_frames)
        , dry_(1u, sample_rate, dry_ring_frames)
    {
    }

    bool RecordingBus::arm(const std::string& folder, const std::string& take_name)
    {
        if (!processed_.arm(join_path(folder, take_name + "-processed.wav")))
        {
            return false;
        }
        if (!dry_.arm(join_path(folder, take_name + "-dry.wav")))
        {
            processed_.stop();
            return false;
        }

        dropped_blocks_.store(0, std::memory_order_relaxed);
        return true;
    }

    bool RecordingBus::write(const float* const* processed_channels, const float* dry_channel,
                             std::uint32_t frames) noexcept
    {
        const bool armed = processed_.is_recording() && dry_.is_recording();
        const bool accepted = armed && processed_.has_room(frames) && dry_.has_room(frames);

        block_index_.fetch_add(1, std::memory_order_relaxed);

        if (!accepted)
        {
            if (armed)
            {
                dropped_blocks_.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        const float* dry_channels[1] = { dry_channel };
        const bool   processed_queued = processed_.write(processed_channels, frames);
        const bool   dry_queued = dry_.write(dry_channels, frames);
        return processed_queued && dry_queued;
    }

    void RecordingBus::stop()
    {
        // Both legs refuse further blocks before either closes, so neither file outlives the other.
        processed_.disarm();
        dry_.disarm();
        processed_.stop();
        dry_.stop();
    }
}
