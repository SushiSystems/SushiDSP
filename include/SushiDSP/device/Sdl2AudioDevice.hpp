/**************************************************************************/
/* Sdl2AudioDevice.hpp                                                    */
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

#include "SushiDSP/core/IAudioDevice.hpp"

#include <cstdint>
#include <vector>

struct SDL_AudioSpec;

namespace SushiDSP
{
    /// @brief SDL2-backed IAudioDevice. The only class in SushiDSP that touches SDL2 —
    /// no SDL type or header leaks past this class's own .cpp file.
    class Sdl2AudioDevice : public IAudioDevice
    {
    public:
        /// @brief Opens the default SDL2 audio playback device at the requested format.
        /// @param requested_format The sample rate, block size, and channel count to request.
        explicit Sdl2AudioDevice(const DeviceFormat& requested_format);

        ~Sdl2AudioDevice() override;

        DeviceFormat format() const override;

        void start(DeviceCallback callback) override;

        void stop() override;

    private:
        /// @brief Holds the format the device actually opened at.
        DeviceFormat format_;

        /// @brief Identifies the open SDL2 device; 0 when none is open.
        unsigned int device_id_;

        /// @brief Holds the caller's per-block callback, empty while the device is stopped.
        DeviceCallback callback_;

        /// @brief Holds one deinterleaved output channel after another, block_size frames each.
        /// Empty for a one-channel device, which writes SDL2's buffer directly.
        std::vector<float> deinterleaved_output_;

        /// @brief Points at each channel's span of deinterleaved_output_, in channel order.
        std::vector<float*> output_channel_pointers_;

        /// @brief Interleaves one block of channels into a destination frame by frame.
        /// @param channels One pointer per channel, each holding `frames` samples.
        /// @param channel_count The number of channels, matching the destination's stride.
        /// @param frames Sample count per channel.
        /// @param destination The interleaved buffer, channel_count * frames samples.
        static void interleave(const float* const* channels, std::uint32_t channel_count,
                               std::uint32_t frames, float* destination);

        /// @brief Serves SDL2's audio thread by running the caller's callback into `stream`.
        /// @param userdata The Sdl2AudioDevice that opened the device.
        /// @param stream SDL2's interleaved output buffer.
        /// @param length_bytes The buffer's size in bytes.
        static void sdl_callback(void* userdata, unsigned char* stream, int length_bytes);
    };
}
