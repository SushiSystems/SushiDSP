/**************************************************************************/
/* asio_audio_device.cpp                                                  */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file asio_audio_device.cpp
 * @brief Implements the ASIO-backed audio device.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/device/asio_audio_device.hpp>

#include <SushiDSP/foundation/core/denormal_guard.hpp>
#include <SushiDSP/foundation/device/device_log_category.hpp>
#include <SushiDSP/foundation/log/log.hpp>

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <windows.h>

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

// asiodrivers.cpp defines loadAsioDriver and no SDK header declares it (modules/foundation/device/README.md).
extern bool loadAsioDriver(char* name);

namespace SushiDSP
{
    namespace
    {
        /// @brief The SDK's getDriverNames() writes at most 32 bytes per name.
        constexpr int K_ASIO_DRIVER_NAME_BYTES = 32;

        /// @brief More ASIO drivers than any machine plausibly registers; the enumeration
        /// buffer is stack-allocated, so it needs a fixed bound.
        constexpr int K_MAX_ASIO_DRIVERS = 64;

        /// @brief Writes one WARN when ASIOStop returned an error; the stream is then treated as
        /// stopped.
        void log_stop_error(ASIOError error)
        {
            if (error != ASE_OK)
            {
                SD_LOG_WARN(K_DEVICE_LOG_CATEGORY,
                            "ASIOStop failed (ASIOError {}); the stream is treated as stopped",
                            static_cast<long>(error));
            }
        }

        void throw_on_error(ASIOError error, const char* what)
        {
            if (error != ASE_OK)
            {
                throw std::runtime_error(std::string(what) + " failed (ASIOError " +
                                         std::to_string(static_cast<long>(error)) + ")");
            }
        }

        /// @brief Maps the driver's native sample type onto the SDK-independent enum Task 3
        /// converts through. Anything outside the three supported containers is rejected by
        /// name rather than silently misread, matching WavFile's convention.
        AsioSampleFormat to_sample_format(ASIOSampleType type)
        {
            switch (type)
            {
            case ASIOSTInt16LSB:
                return AsioSampleFormat::Int16LSB;
            case ASIOSTInt32LSB:
                return AsioSampleFormat::Int32LSB;
            case ASIOSTFloat32LSB:
                return AsioSampleFormat::Float32LSB;
            default:
                break;
            }

            throw std::runtime_error(
                "Unsupported ASIOSampleType " + std::to_string(static_cast<long>(type)) +
                "; SushiDSP supports ASIOSTInt16LSB, ASIOSTInt32LSB and ASIOSTFloat32LSB only");
        }

        std::size_t sample_size_bytes(AsioSampleFormat format)
        {
            return format == AsioSampleFormat::Int16LSB ? 2u : 4u;
        }

        /// @brief True if size is one of the buffer sizes the driver reported as usable.
        /// granularity == -1 means "powers of two between min and max", per asio.h.
        bool is_supported_buffer_size(long size, long min_size, long max_size, long granularity)
        {
            if (size < min_size || size > max_size)
            {
                return false;
            }
            if (granularity == -1)
            {
                return (size & (size - 1)) == 0;
            }
            if (granularity <= 0)
            {
                return size == min_size;
            }
            return ((size - min_size) % granularity) == 0;
        }
    } // namespace

    /// @brief Every ASIO-SDK-typed member, plus the four free-function trampolines the
    /// driver calls. Living here keeps the SDK headers out of asio_audio_device.hpp.
    struct AsioAudioDevice::Impl
    {
            std::vector<ASIOBufferInfo> buffer_infos; // inputs first, then outputs
            ASIOCallbacks callbacks{};
            bool post_output = false; // driver benefits from ASIOOutputReady

            static void buffer_switch(long buffer_index, ASIOBool /*direct_process*/)
            {
                if (AsioAudioDevice* self = AsioAudioDevice::active_instance_)
                {
                    self->on_buffer_switch(buffer_index);
                }
                else
                {
                    AsioAudioDevice::orphan_switches_.increment();
                }
            }

            static void sample_rate_did_change(ASIOSampleRate /*sample_rate*/)
            {
                // A clock change is counted and not followed; format() keeps the first rate.
                if (AsioAudioDevice* self = AsioAudioDevice::active_instance_)
                {
                    self->counters_.driver_message.increment();
                }
            }

            static long asio_message(long selector, long value, void* /*message*/, double* /*opt*/)
            {
                switch (selector)
                {
                case kAsioSelectorSupported:
                    return (value == kAsioEngineVersion || value == kAsioResyncRequest ||
                            value == kAsioLatenciesChanged || value == kAsioOverload)
                               ? 1L
                               : 0L;

                case kAsioEngineVersion:
                    return 2L;

                // Acknowledged and counted, not acted on; reopening is the host's call.
                case kAsioResyncRequest:
                case kAsioLatenciesChanged:
                case kAsioOverload:
                    count_driver_message();
                    return 1L;

                // Answers 0, not handled: this device never rebuilds its buffers (modules/foundation/device/README.md).
                case kAsioResetRequest:
                    count_driver_message();
                    return 0L;

                // Declining time info keeps the driver on the plain bufferSwitch path.
                case kAsioSupportsTimeInfo:
                    return 0L;

                default:
                    return 0L;
                }
            }

            /// @brief Counts a driver message on the open device, if there is one.
            static void count_driver_message() noexcept
            {
                if (AsioAudioDevice* self = AsioAudioDevice::active_instance_)
                {
                    self->counters_.driver_message.increment();
                }
            }

            static ASIOTime* buffer_switch_time_info(ASIOTime* /*params*/, long buffer_index,
                                                     ASIOBool direct_process)
            {
                // Reached only when a driver ignores the kAsioSupportsTimeInfo answer; forwards the block.
                buffer_switch(buffer_index, direct_process);
                return nullptr;
            }
    };

    AsioAudioDevice* AsioAudioDevice::active_instance_ = nullptr;

    RtCounter AsioAudioDevice::orphan_switches_;

    const RtCounter& AsioAudioDevice::orphan_switches() noexcept
    {
        return orphan_switches_;
    }

    std::vector<std::string> AsioAudioDevice::enumerate_drivers()
    {
        AsioDrivers drivers;

        char storage[K_MAX_ASIO_DRIVERS][K_ASIO_DRIVER_NAME_BYTES];
        char* name_pointers[K_MAX_ASIO_DRIVERS];
        for (int i = 0; i < K_MAX_ASIO_DRIVERS; ++i)
        {
            storage[i][0] = '\0';
            name_pointers[i] = storage[i];
        }

        const long count = drivers.getDriverNames(name_pointers, K_MAX_ASIO_DRIVERS);

        std::vector<std::string> names;
        names.reserve(static_cast<std::size_t>(count > 0 ? count : 0));
        for (long i = 0; i < count; ++i)
        {
            storage[i][K_ASIO_DRIVER_NAME_BYTES - 1] = '\0';
            names.emplace_back(storage[i]);
        }
        return names;
    }

    AsioAudioDevice::AsioAudioDevice(const std::string& driver_name,
                                     const DeviceFormat& requested_format)
        : impl_(std::make_unique<Impl>()), format_(requested_format), monitor_(counters_),
          input_sample_format_(AsioSampleFormat::Float32LSB),
          output_sample_format_(AsioSampleFormat::Float32LSB)
    {
        std::string mutable_name = driver_name; // loadAsioDriver takes a non-const char*
        if (!loadAsioDriver(mutable_name.data()))
        {
            throw std::runtime_error("ASIO driver '" + driver_name + "' could not be loaded");
        }

        // Any failure from here on unloads the driver, or the next open of any driver fails.
        try
        {
            ASIODriverInfo driver_info{};
            driver_info.asioVersion = 2;
            // The desktop window stands in for the application window a console host lacks.
            driver_info.sysRef = GetDesktopWindow();
            throw_on_error(ASIOInit(&driver_info), "ASIOInit");

            long available_inputs = 0;
            long available_outputs = 0;
            throw_on_error(ASIOGetChannels(&available_inputs, &available_outputs),
                           "ASIOGetChannels");

            if (static_cast<long>(requested_format.input_channels) > available_inputs ||
                static_cast<long>(requested_format.output_channels) > available_outputs)
            {
                throw std::runtime_error(
                    "ASIO driver '" + driver_name + "' offers " + std::to_string(available_inputs) +
                    " input and " + std::to_string(available_outputs) + " output channels; " +
                    std::to_string(requested_format.input_channels) + "/" +
                    std::to_string(requested_format.output_channels) + " requested");
            }

            long min_size = 0;
            long max_size = 0;
            long preferred_size = 0;
            long granularity = 0;
            throw_on_error(ASIOGetBufferSize(&min_size, &max_size, &preferred_size, &granularity),
                           "ASIOGetBufferSize");

            const long requested_block = static_cast<long>(requested_format.block_size);
            const long block_size =
                is_supported_buffer_size(requested_block, min_size, max_size, granularity)
                    ? requested_block
                    : preferred_size;
            if (block_size != requested_block)
            {
                SD_LOG_WARN(K_DEVICE_LOG_CATEGORY,
                            "ASIO driver '{}' cannot run blocks of {} frames (it accepts {} to {}); "
                            "using its preferred {} frames",
                            driver_name, requested_block, min_size, max_size, block_size);
            }

            // The rate is set only once ASIOCanSampleRate confirms it (modules/foundation/device/README.md).
            if (requested_format.sample_rate > 0.0 && ASIOCanSampleRate(static_cast<ASIOSampleRate>(
                                                          requested_format.sample_rate)) == ASE_OK)
            {
                throw_on_error(
                    ASIOSetSampleRate(static_cast<ASIOSampleRate>(requested_format.sample_rate)),
                    "ASIOSetSampleRate");
            }

            ASIOSampleRate actual_rate = 0.0;
            throw_on_error(ASIOGetSampleRate(&actual_rate), "ASIOGetSampleRate");
            if (actual_rate <= 0.0)
            {
                throw std::runtime_error("ASIO driver '" + driver_name +
                                         "' reports no valid sample rate (no clock)");
            }

            if (requested_format.sample_rate > 0.0 &&
                std::abs(static_cast<double>(actual_rate) - requested_format.sample_rate) > 0.5)
            {
                SD_LOG_WARN(K_DEVICE_LOG_CATEGORY,
                            "ASIO driver '{}' cannot run at {} Hz; using its own clock at {} Hz",
                            driver_name, requested_format.sample_rate,
                            static_cast<double>(actual_rate));
            }

            format_.sample_rate = static_cast<double>(actual_rate);
            format_.block_size = static_cast<std::uint32_t>(block_size);
            format_.input_channels = requested_format.input_channels;
            format_.output_channels = requested_format.output_channels;

            // One container format per direction; a mixed-format driver is rejected (modules/foundation/device/README.md).
            const auto probe_direction =
                [](long channel_count, ASIOBool is_input, const char* label)
            {
                AsioSampleFormat format = AsioSampleFormat::Float32LSB;
                for (long channel = 0; channel < channel_count; ++channel)
                {
                    ASIOChannelInfo info{};
                    info.channel = channel;
                    info.isInput = is_input;
                    throw_on_error(ASIOGetChannelInfo(&info), "ASIOGetChannelInfo");

                    const AsioSampleFormat channel_format = to_sample_format(info.type);
                    if (channel > 0 && channel_format != format)
                    {
                        throw std::runtime_error(
                            std::string("ASIO driver mixes sample formats across its ") + label +
                            " channels");
                    }
                    format = channel_format;
                }
                return format;
            };

            input_sample_format_ =
                probe_direction(static_cast<long>(format_.input_channels), ASIOTrue, "input");
            output_sample_format_ =
                probe_direction(static_cast<long>(format_.output_channels), ASIOFalse, "output");

            const std::size_t total_channels = format_.input_channels + format_.output_channels;
            impl_->buffer_infos.resize(total_channels);
            for (std::uint32_t channel = 0; channel < format_.input_channels; ++channel)
            {
                impl_->buffer_infos[channel].isInput = ASIOTrue;
                impl_->buffer_infos[channel].channelNum = static_cast<long>(channel);
            }
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                ASIOBufferInfo& info = impl_->buffer_infos[format_.input_channels + channel];
                info.isInput = ASIOFalse;
                info.channelNum = static_cast<long>(channel);
            }

            impl_->callbacks.bufferSwitch = &Impl::buffer_switch;
            impl_->callbacks.sampleRateDidChange = &Impl::sample_rate_did_change;
            impl_->callbacks.asioMessage = &Impl::asio_message;
            impl_->callbacks.bufferSwitchTimeInfo = &Impl::buffer_switch_time_info;

            // active_instance_ is set before ASIOCreateBuffers, which may call asioMessage.
            active_instance_ = this;

            throw_on_error(ASIOCreateBuffers(impl_->buffer_infos.data(),
                                             static_cast<long>(total_channels), block_size,
                                             &impl_->callbacks),
                           "ASIOCreateBuffers");

            impl_->post_output = (ASIOOutputReady() == ASE_OK);

            // All real-time storage is allocated exactly once, here.
            input_scratch_.assign(format_.input_channels,
                                  std::vector<float>(format_.block_size, 0.0f));
            output_scratch_.assign(format_.output_channels,
                                   std::vector<float>(format_.block_size, 0.0f));
            input_scratch_pointers_.resize(format_.input_channels);
            output_scratch_pointers_.resize(format_.output_channels);
            for (std::uint32_t channel = 0; channel < format_.input_channels; ++channel)
            {
                input_scratch_pointers_[channel] = input_scratch_[channel].data();
            }
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                output_scratch_pointers_[channel] = output_scratch_[channel].data();
            }
        }
        catch (...)
        {
            active_instance_ = nullptr;
            ASIOExit();
            throw;
        }
    }

    AsioAudioDevice::~AsioAudioDevice()
    {
        if (running_)
        {
            log_stop_error(ASIOStop());
            running_ = false;
        }
        ASIODisposeBuffers();
        ASIOExit();
        active_instance_ = nullptr;
    }

    DeviceFormat AsioAudioDevice::format() const
    {
        return format_;
    }

    void AsioAudioDevice::start(DeviceCallback callback)
    {
        if (running_)
        {
            return;
        }

        // Installed before ASIOStart, so the first buffer switch already sees it.
        callback_ = std::move(callback);

        const ASIOError error = ASIOStart();
        if (error != ASE_OK)
        {
            callback_ = {};
            throw_on_error(error, "ASIOStart");
        }
        running_ = true;
    }

    void AsioAudioDevice::stop()
    {
        if (!running_)
        {
            return;
        }

        // ASIOStop returns with no further bufferSwitch to come, so the callback is released after it.
        log_stop_error(ASIOStop());
        running_ = false;
        callback_ = {};
    }

    void AsioAudioDevice::add_counters_to(RtCounterReporter& reporter) const
    {
        counters_.add_to(reporter);
        reporter.add("orphan_switch", orphan_switches_);
    }

    const DeviceCounters& AsioAudioDevice::counters() const noexcept
    {
        return counters_;
    }

    void AsioAudioDevice::on_buffer_switch(long buffer_index)
    {
        DenormalGuard denormal_guard;
        const std::uint32_t frames = format_.block_size;
        const BlockScope block_scope(monitor_, frames, format_.sample_rate);
        const std::size_t index = static_cast<std::size_t>(buffer_index);

        const std::size_t input_bytes = sample_size_bytes(input_sample_format_);
        for (std::uint32_t channel = 0; channel < format_.input_channels; ++channel)
        {
            const auto* source =
                static_cast<const unsigned char*>(impl_->buffer_infos[channel].buffers[index]);
            float* destination = input_scratch_[channel].data();
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                destination[frame] =
                    asio_sample_to_float(input_sample_format_, source + frame * input_bytes);
            }
        }

        if (callback_)
        {
            // AudioDevice's contract is that a device with no capture passes nullptr.
            const float* const* inputs =
                format_.input_channels > 0 ? input_scratch_pointers_.data() : nullptr;
            callback_(inputs, output_scratch_pointers_.data(), frames);
        }
        else
        {
            counters_.no_callback.increment();
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                std::memset(output_scratch_[channel].data(), 0, frames * sizeof(float));
            }
        }

        const std::size_t output_bytes = sample_size_bytes(output_sample_format_);
        for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
        {
            const std::size_t slot = static_cast<std::size_t>(format_.input_channels) + channel;
            auto* destination =
                static_cast<unsigned char*>(impl_->buffer_infos[slot].buffers[index]);
            const float* source = output_scratch_[channel].data();
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                float_to_asio_sample(output_sample_format_, source[frame],
                                     destination + frame * output_bytes);
            }
        }

        if (impl_->post_output && ASIOOutputReady() != ASE_OK)
        {
            counters_.driver_error.increment();
        }
    }
} // namespace SushiDSP
