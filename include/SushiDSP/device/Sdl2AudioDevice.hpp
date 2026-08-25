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
        DeviceFormat   format_;
        unsigned int   device_id_;
        DeviceCallback callback_;

        static void sdl_callback(void* userdata, unsigned char* stream, int length_bytes);
    };
}
