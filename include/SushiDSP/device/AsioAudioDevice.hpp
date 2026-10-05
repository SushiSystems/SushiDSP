/****************************************************************/
/* AsioAudioDevice.hpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file AsioAudioDevice.hpp
 * @brief Declares the ASIO-backed audio device.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "SushiDSP/core/IAudioDevice.hpp"
#include "SushiDSP/audio_format/AsioSampleConverter.hpp"

namespace SushiDSP
{
    /// @brief ASIO-backed IAudioDevice. The only class in SushiDSP that touches ASIO SDK
    /// types - no ASIO header leaks past this class's own .cpp file, which is why every
    /// SDK-typed member lives behind the opaque Impl below rather than in this header.
    /// Windows-only; only built when the ASIO SDK is present (see CMakeLists.txt's
    /// SUSHIDSP_BUILD_ASIO block). Duplex: reports genuine input_channels alongside
    /// output_channels.
    class AsioAudioDevice : public IAudioDevice
    {
    public:
        /// @brief Lists the ASIO driver names currently registered on this machine.
        /// @return Driver names, suitable for passing to the constructor.
        static std::vector<std::string> enumerate_drivers();

        /// @param driver_name An exact name from enumerate_drivers().
        /// @param requested_format sample_rate/block_size are requests; the opened device's
        /// actual negotiated format is returned by format() and may differ. input_channels/
        /// output_channels select how many of the driver's hardware channels (starting from
        /// channel 0) this device presents through IAudioDevice.
        /// @throws std::runtime_error if the driver cannot be loaded or initialized.
        AsioAudioDevice(const std::string& driver_name, const DeviceFormat& requested_format);

        /// @brief Stops the stream if running, releases the driver's buffers, and unloads it.
        ~AsioAudioDevice() override;

        AsioAudioDevice(const AsioAudioDevice&) = delete;
        AsioAudioDevice& operator=(const AsioAudioDevice&) = delete;

        /// @brief The negotiated format the driver actually opened with.
        /// @return This device's DeviceFormat.
        DeviceFormat format() const override;

        /// @brief Starts the driver's real-time stream, invoking callback once per block.
        /// @param callback The per-block render callback.
        /// @throws std::runtime_error if ASIOStart fails.
        void start(DeviceCallback callback) override;

        /// @brief Stops the driver's real-time stream and releases the callback.
        void stop() override;

    private:
        /// @brief Holds every ASIO-SDK-typed member (buffer infos, the callback struct)
        /// and the static SDK trampolines, so no ASIO header is needed to include this file.
        struct Impl;
        friend struct Impl;

        std::unique_ptr<Impl> impl_;

        DeviceFormat   format_;
        DeviceCallback callback_;

        AsioSampleFormat input_sample_format_;
        AsioSampleFormat output_sample_format_;

        std::vector<std::vector<float>> input_scratch_;   // [channel][frame], deinterleaved
        std::vector<std::vector<float>> output_scratch_;  // [channel][frame], deinterleaved
        std::vector<const float*>       input_scratch_pointers_;
        std::vector<float*>             output_scratch_pointers_;

        bool running_ = false;

        /// @brief Points at the one open device the ASIO free-function callbacks forward to.
        /// @pre At most one AsioAudioDevice is open at a time (include/SushiDSP/README.md).
        static AsioAudioDevice* active_instance_;

        /// @brief The real-time buffer-switch body. Allocation-, lock-, syscall- and
        /// throw-free: every buffer it touches was sized in the constructor.
        void on_buffer_switch(long buffer_index);
    };
}
