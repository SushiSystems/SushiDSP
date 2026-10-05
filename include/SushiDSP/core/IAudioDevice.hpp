/****************************************************************/
/* IAudioDevice.hpp                                             */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstdint>
#include <functional>

namespace SushiDSP
{
    /// @brief The sample rate, block size, and channel counts a device is configured for.
    struct DeviceFormat
    {
        double        sample_rate;
        std::uint32_t block_size;
        std::uint32_t input_channels;
        std::uint32_t output_channels;
    };

    /// @brief Invoked by an IAudioDevice on its real-time thread once per block. Must fill
    /// exactly frames samples of each of outputs[0..output_channels). inputs may be nullptr,
    /// or point to input_channels channels of captured audio, or be non-null with fewer
    /// meaningful channels than input_channels reports (a device with zero input capability
    /// reports input_channels == 0 and always passes nullptr). The callback itself must be
    /// allocation-free, lock-free, syscall-free, and throw-free.
    using DeviceCallback = std::function<void(const float* const* inputs, float* const* outputs, std::uint32_t frames)>;

    /// @brief A hardware or host output device. Injected — SushiDSP never assumes one exists.
    struct IAudioDevice
    {
        virtual ~IAudioDevice() = default;

        /// @brief The sample rate, block size, and channel counts this device is configured for.
        /// @return The device's DeviceFormat.
        virtual DeviceFormat format() const = 0;

        /// @brief Begins real-time playback, invoking callback on the device's audio thread.
        /// @param callback The per-block render callback.
        virtual void start(DeviceCallback callback) = 0;

        /// @brief Stops playback and joins the audio thread.
        virtual void stop() = 0;
    };
}
