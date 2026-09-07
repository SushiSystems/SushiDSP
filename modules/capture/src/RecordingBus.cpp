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

    RecordingBus::RecordingBus(std::uint32_t processed_channels, double sample_rate)
        : processed_(processed_channels, sample_rate, kRingCapacityFrames)
        , dry_(1u, sample_rate, kRingCapacityFrames)
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

        armed_block_index_ = block_index_.load(std::memory_order_relaxed);
        return true;
    }

    bool RecordingBus::write_processed(const float* const* channels, std::uint32_t frames) noexcept
    {
        const bool queued = processed_.write(channels, frames);
        block_index_.fetch_add(1, std::memory_order_relaxed);
        return queued;
    }

    bool RecordingBus::write_dry(const float* const* channels, std::uint32_t frames) noexcept
    {
        return dry_.write(channels, frames);
    }

    void RecordingBus::stop()
    {
        processed_.stop();
        dry_.stop();
    }
}
