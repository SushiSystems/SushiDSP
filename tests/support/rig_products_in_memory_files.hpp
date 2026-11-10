/**************************************************************************/
/* rig_products_in_memory_files.hpp                                       */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file rig_products_in_memory_files.hpp
 * @brief Declares the fixture that builds rigs and sessions on audio and text files kept in memory.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SushiDSP/nodes/capture/take.hpp>

#include "audio_files.hpp"
#include "hrtf_files.hpp"
#include "rig_controller.hpp"
#include "session.hpp"
#include "session_timeline.hpp"
#include "support/in_memory_room.hpp"
#include "support/rig_products.hpp"
#include "text_files.hpp"

namespace TestSupport
{
    /// @brief RigProducts whose rigs read takes from, and record into, an InMemoryAudioFiles the
    /// fixture owns, so no case of it touches a file. A case adds its takes with add_wav() and its
    /// HRTF assets with add_hrtf().
    class RigProductsInMemoryFiles : public RigProducts
    {
        public:
            RigProductsInMemoryFiles() = default;

        protected:
            /// @param room The descriptor listed under the room's id.
            explicit RigProductsInMemoryFiles(const SushiDSP::NodeDescriptor& room)
                : RigProducts(room)
            {
            }

            /// @brief Adds an HRTF asset under a name.
            /// @param name The asset's name; it is the last part of the path returned.
            /// @param bytes The asset's content.
            /// @return The path the rigs read the asset from.
            std::string add_hrtf(const std::string& name, std::vector<unsigned char> bytes)
            {
                const std::string path = "memory/" + name;
                hrtf_files_.add(path, std::move(bytes));
                return path;
            }

            /// @brief Adds a mono take under a name.
            /// @param name The file's name; it is the last part of the path returned.
            /// @param samples The audio.
            /// @param sample_rate The rate in Hz.
            /// @return The path the rigs read the take from.
            std::string add_wav(const std::string& name, const std::vector<float>& samples,
                                double sample_rate)
            {
                const std::string path = "memory/" + name;
                files_.add(path, sample_rate, {samples});
                return path;
            }

            /// @brief Builds a rig on a timeline that reads and records through the fixture's
            /// files.
            /// @param info The sample rate, block size and channel count.
            /// @param clock Returns the present time for the room publish interval; empty reads the
            /// steady clock.
            /// @return The rig; it must not outlive the fixture.
            RigController rig(const SushiDSP::PrepareInfo& info,
                              RigController::ClockSource clock = {})
            {
                return RigController(info, std::make_shared<SessionTimeline>(info, files_),
                                     std::move(clock), catalogue(), files_, hrtf_files_);
            }

            /// @brief Returns the files the rigs of this fixture read and write.
            SushiDSP::host::InMemoryAudioFiles& files() noexcept
            {
                return files_;
            }

            /// @brief Returns the text files the preset and project functions of this fixture
            /// read and write.
            SushiDSP::host::InMemoryTextFiles& text_files() noexcept
            {
                return text_files_;
            }

        private:
            SushiDSP::host::InMemoryAudioFiles files_;
            SushiDSP::host::InMemoryHrtfFiles hrtf_files_;
            SushiDSP::host::InMemoryTextFiles text_files_;
    };

    /// @brief RigProductsInMemoryFiles with the room swapped for its in-memory adapter, for a test
    /// that reads the files a rig loads and nothing the room renders.
    class RigProductsInMemoryRoomFiles : public RigProductsInMemoryFiles
    {
        public:
            RigProductsInMemoryRoomFiles() : RigProductsInMemoryFiles(in_memory_room_descriptor())
            {
            }
    };
} // namespace TestSupport
