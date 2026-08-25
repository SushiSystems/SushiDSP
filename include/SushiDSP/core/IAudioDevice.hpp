/**************************************************************************/
/* IAudioDevice.hpp                                                       */
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

#include <cstdint>
#include <functional>

namespace SushiDSP
{
    /// @brief The sample rate, block size, and channel count a device is configured for.
    struct DeviceFormat
    {
        double        sample_rate;
        std::uint32_t block_size;
        std::uint32_t channels;
    };

    /// @brief Invoked by an IAudioDevice on its real-time thread once per block. Must fill
    /// exactly frames samples of each of outputs[0..channels). The callback itself must be
    /// allocation-free, lock-free, syscall-free, and throw-free.
    using DeviceCallback = std::function<void(float* const* outputs, std::uint32_t frames)>;

    /// @brief A hardware or host output device. Injected — SushiDSP never assumes one exists.
    struct IAudioDevice
    {
        virtual ~IAudioDevice() = default;

        /// @brief The sample rate, block size, and channel count this device is configured for.
        /// @return The device's DeviceFormat.
        virtual DeviceFormat format() const = 0;

        /// @brief Begins real-time playback, invoking callback on the device's audio thread.
        /// @param callback The per-block render callback.
        virtual void start(DeviceCallback callback) = 0;

        /// @brief Stops playback and joins the audio thread.
        virtual void stop() = 0;
    };
}
