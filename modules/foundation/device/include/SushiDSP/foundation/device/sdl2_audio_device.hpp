/**************************************************************************/
/* sdl2_audio_device.hpp                                                  */
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
 * @file sdl2_audio_device.hpp
 * @brief Declares the SDL2-backed audio device.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/device/block_server.hpp>
#include <SushiDSP/foundation/device/device_counters.hpp>

#include <memory>

struct SDL_AudioSpec;

namespace SushiDSP
{
    /// @brief SDL2-backed AudioDevice. The only class in SushiDSP that touches SDL2 —
    /// no SDL type or header leaks past this class's own .cpp file.
    class Sdl2AudioDevice : public AudioDevice
    {
        public:
            /// @brief Opens the default SDL2 audio playback device at the requested format.
            /// @param requested_format The sample rate, block size, and channel count to request.
            explicit Sdl2AudioDevice(const DeviceFormat& requested_format);

            ~Sdl2AudioDevice() override;

            DeviceFormat format() const override;

            void start(DeviceCallback callback) override;

            void stop() override;

            /// @brief Adds this device's counters to @p reporter.
            void add_counters_to(RtCounterReporter& reporter) const override;

            /// @brief Returns what the audio thread counted; read it on the control thread.
            [[nodiscard]] const DeviceCounters& counters() const noexcept;

        private:
            /// @brief Holds the format the device actually opened at.
            DeviceFormat format_;

            /// @brief Identifies the open SDL2 device; 0 when none is open.
            unsigned int device_id_;

            /// @brief Answers each block SDL2 asks for; made once the device has opened at its real format.
            std::unique_ptr<BlockServer> server_;

            /// @brief Serves SDL2's audio thread by running the caller's callback into `stream`.
            /// @param userdata The Sdl2AudioDevice that opened the device.
            /// @param stream SDL2's interleaved output buffer.
            /// @param length_bytes The buffer's size in bytes.
            static void sdl_callback(void* userdata, unsigned char* stream, int length_bytes);
    };
} // namespace SushiDSP
