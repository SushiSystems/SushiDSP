/**************************************************************************/
/* AsioAudioDevice.cpp                                                    */
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

#include "SushiDSP/device/AsioAudioDevice.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

#include <windows.h>

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

// asiodrivers.cpp defines this but declares it in no header; the SDK's own host sample
// declares it locally too, so this is the sanctioned way to reach it.
extern bool loadAsioDriver(char* name);

namespace SushiDSP
{
    namespace
    {
        /// @brief The SDK's getDriverNames() writes at most 32 bytes per name.
        constexpr int kAsioDriverNameBytes = 32;

        /// @brief More ASIO drivers than any machine plausibly registers; the enumeration
        /// buffer is stack-allocated, so it needs a fixed bound.
        constexpr int kMaxAsioDrivers = 64;

        void throw_on_error(ASIOError error, const char* what)
        {
            if (error != ASE_OK)
            {
                throw std::runtime_error(std::string(what) + " failed (ASIOError " + std::to_string(static_cast<long>(error)) + ")");
            }
        }

        /// @brief Maps the driver's native sample type onto the SDK-independent enum Task 3
        /// converts through. Anything outside the three supported containers is rejected by
        /// name rather than silently misread, matching WavFile's convention.
        AsioSampleFormat to_sample_format(ASIOSampleType type)
        {
            switch (type)
            {
                case ASIOSTInt16LSB:   return AsioSampleFormat::Int16LSB;
                case ASIOSTInt32LSB:   return AsioSampleFormat::Int32LSB;
                case ASIOSTFloat32LSB: return AsioSampleFormat::Float32LSB;
                default: break;
            }

            throw std::runtime_error("Unsupported ASIOSampleType " + std::to_string(static_cast<long>(type))
                                     + "; SushiDSP supports ASIOSTInt16LSB, ASIOSTInt32LSB and ASIOSTFloat32LSB only");
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
    }

    /// @brief Every ASIO-SDK-typed member, plus the four free-function trampolines the
    /// driver calls. Living here keeps the SDK headers out of AsioAudioDevice.hpp.
    struct AsioAudioDevice::Impl
    {
        std::vector<ASIOBufferInfo> buffer_infos;  // inputs first, then outputs
        ASIOCallbacks               callbacks{};
        bool                        post_output = false;  // driver benefits from ASIOOutputReady

        static void buffer_switch(long buffer_index, ASIOBool /*direct_process*/)
        {
            if (AsioAudioDevice* self = AsioAudioDevice::active_instance_)
            {
                self->on_buffer_switch(buffer_index);
            }
        }

        static void sample_rate_did_change(ASIOSampleRate /*sample_rate*/)
        {
            // Nothing to do: format() reports the rate negotiated at construction, and a
            // mid-stream clock change is a driver-level event this device does not chase.
        }

        static long asio_message(long selector, long value, void* /*message*/, double* /*opt*/)
        {
            switch (selector)
            {
                case kAsioSelectorSupported:
                    return (value == kAsioEngineVersion
                            || value == kAsioResyncRequest || value == kAsioLatenciesChanged
                            || value == kAsioOverload) ? 1L : 0L;

                case kAsioEngineVersion:
                    return 2L;

                // Acknowledged so the driver does not keep re-asking; this device does not
                // act on them, since reopening the driver is the host application's call.
                case kAsioResyncRequest:
                case kAsioLatenciesChanged:
                case kAsioOverload:
                    return 1L;

                // Answering 1 here would claim this device tears down and re-creates its
                // buffers in response, which it does not do - a real reset handler belongs
                // at the host layer that owns rebuilding format/buffers, which does not
                // exist yet. 0 is the honest "not handled" answer, and correctly excluded
                // from kAsioSelectorSupported above.
                case kAsioResetRequest:
                    return 0L;

                // Declining time info keeps the driver on the plain bufferSwitch path,
                // which is all this device needs.
                case kAsioSupportsTimeInfo:
                    return 0L;

                default:
                    return 0L;
            }
        }

        static ASIOTime* buffer_switch_time_info(ASIOTime* /*params*/, long buffer_index, ASIOBool direct_process)
        {
            // Only reachable if a driver ignores our kAsioSupportsTimeInfo answer; forward
            // it rather than dropping the block.
            buffer_switch(buffer_index, direct_process);
            return nullptr;
        }
    };

    AsioAudioDevice* AsioAudioDevice::active_instance_ = nullptr;

    std::vector<std::string> AsioAudioDevice::enumerate_drivers()
    {
        AsioDrivers drivers;

        char  storage[kMaxAsioDrivers][kAsioDriverNameBytes];
        char* name_pointers[kMaxAsioDrivers];
        for (int i = 0; i < kMaxAsioDrivers; ++i)
        {
            storage[i][0] = '\0';
            name_pointers[i] = storage[i];
        }

        const long count = drivers.getDriverNames(name_pointers, kMaxAsioDrivers);

        std::vector<std::string> names;
        names.reserve(static_cast<std::size_t>(count > 0 ? count : 0));
        for (long i = 0; i < count; ++i)
        {
            storage[i][kAsioDriverNameBytes - 1] = '\0';
            names.emplace_back(storage[i]);
        }
        return names;
    }

    AsioAudioDevice::AsioAudioDevice(const std::string& driver_name, const DeviceFormat& requested_format)
        : impl_(std::make_unique<Impl>())
        , format_(requested_format)
        , input_sample_format_(AsioSampleFormat::Float32LSB)
        , output_sample_format_(AsioSampleFormat::Float32LSB)
    {
        std::string mutable_name = driver_name;  // loadAsioDriver takes a non-const char*
        if (!loadAsioDriver(mutable_name.data()))
        {
            throw std::runtime_error("ASIO driver '" + driver_name + "' could not be loaded");
        }

        // From here on any failure must unload the driver, or the process-wide SDK state
        // stays wedged and the next open of any driver fails.
        try
        {
            ASIODriverInfo driver_info{};
            driver_info.asioVersion = 2;
            // Console and library hosts have no application window; the desktop window is
            // the conventional stand-in, and drivers that ignore sysRef are unaffected.
            driver_info.sysRef = GetDesktopWindow();
            throw_on_error(ASIOInit(&driver_info), "ASIOInit");

            long available_inputs = 0;
            long available_outputs = 0;
            throw_on_error(ASIOGetChannels(&available_inputs, &available_outputs), "ASIOGetChannels");

            if (static_cast<long>(requested_format.input_channels) > available_inputs
                || static_cast<long>(requested_format.output_channels) > available_outputs)
            {
                throw std::runtime_error("ASIO driver '" + driver_name + "' offers "
                                         + std::to_string(available_inputs) + " input and "
                                         + std::to_string(available_outputs) + " output channels; "
                                         + std::to_string(requested_format.input_channels) + "/"
                                         + std::to_string(requested_format.output_channels) + " requested");
            }

            long min_size = 0;
            long max_size = 0;
            long preferred_size = 0;
            long granularity = 0;
            throw_on_error(ASIOGetBufferSize(&min_size, &max_size, &preferred_size, &granularity), "ASIOGetBufferSize");

            const long requested_block = static_cast<long>(requested_format.block_size);
            const long block_size = is_supported_buffer_size(requested_block, min_size, max_size, granularity)
                                        ? requested_block
                                        : preferred_size;

            // ASIOCanSampleRate is the only non-destructive way to ask; only set the rate
            // when the driver confirms it, so an unsupported request leaves the hardware's
            // current clock alone rather than failing the whole open.
            if (requested_format.sample_rate > 0.0
                && ASIOCanSampleRate(static_cast<ASIOSampleRate>(requested_format.sample_rate)) == ASE_OK)
            {
                throw_on_error(ASIOSetSampleRate(static_cast<ASIOSampleRate>(requested_format.sample_rate)), "ASIOSetSampleRate");
            }

            ASIOSampleRate actual_rate = 0.0;
            throw_on_error(ASIOGetSampleRate(&actual_rate), "ASIOGetSampleRate");
            if (actual_rate <= 0.0)
            {
                throw std::runtime_error("ASIO driver '" + driver_name + "' reports no valid sample rate (no clock)");
            }

            format_.sample_rate     = static_cast<double>(actual_rate);
            format_.block_size      = static_cast<std::uint32_t>(block_size);
            format_.input_channels  = requested_format.input_channels;
            format_.output_channels = requested_format.output_channels;

            // Every channel in one direction must share a container format; this device
            // keeps a single per-direction format rather than a per-channel table, so a
            // mixed-format driver is rejected instead of misread.
            const auto probe_direction = [](long channel_count, ASIOBool is_input, const char* label)
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
                        throw std::runtime_error(std::string("ASIO driver mixes sample formats across its ") + label + " channels");
                    }
                    format = channel_format;
                }
                return format;
            };

            input_sample_format_  = probe_direction(static_cast<long>(format_.input_channels), ASIOTrue, "input");
            output_sample_format_ = probe_direction(static_cast<long>(format_.output_channels), ASIOFalse, "output");

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

            impl_->callbacks.bufferSwitch         = &Impl::buffer_switch;
            impl_->callbacks.sampleRateDidChange  = &Impl::sample_rate_did_change;
            impl_->callbacks.asioMessage          = &Impl::asio_message;
            impl_->callbacks.bufferSwitchTimeInfo = &Impl::buffer_switch_time_info;

            // active_instance_ must be live before ASIOCreateBuffers: some drivers issue
            // asioMessage callbacks from inside it.
            active_instance_ = this;

            throw_on_error(ASIOCreateBuffers(impl_->buffer_infos.data(),
                                             static_cast<long>(total_channels),
                                             block_size,
                                             &impl_->callbacks),
                           "ASIOCreateBuffers");

            impl_->post_output = (ASIOOutputReady() == ASE_OK);

            // All real-time storage is allocated exactly once, here.
            input_scratch_.assign(format_.input_channels, std::vector<float>(format_.block_size, 0.0f));
            output_scratch_.assign(format_.output_channels, std::vector<float>(format_.block_size, 0.0f));
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
            ASIOStop();
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

        // ASIOStop guarantees no further bufferSwitch call on return, so releasing whatever
        // the caller's callback captured is safe only after it.
        ASIOStop();
        running_ = false;
        callback_ = {};
    }

    void AsioAudioDevice::on_buffer_switch(long buffer_index)
    {
        const std::uint32_t frames = format_.block_size;
        const std::size_t   index = static_cast<std::size_t>(buffer_index);

        const std::size_t input_bytes = sample_size_bytes(input_sample_format_);
        for (std::uint32_t channel = 0; channel < format_.input_channels; ++channel)
        {
            const auto* source = static_cast<const unsigned char*>(impl_->buffer_infos[channel].buffers[index]);
            float* destination = input_scratch_[channel].data();
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                destination[frame] = asio_sample_to_float(input_sample_format_, source + frame * input_bytes);
            }
        }

        if (callback_)
        {
            // IAudioDevice's contract is that a device with no capture passes nullptr.
            const float* const* inputs = format_.input_channels > 0 ? input_scratch_pointers_.data() : nullptr;
            callback_(inputs, output_scratch_pointers_.data(), frames);
        }
        else
        {
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                std::memset(output_scratch_[channel].data(), 0, frames * sizeof(float));
            }
        }

        const std::size_t output_bytes = sample_size_bytes(output_sample_format_);
        for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
        {
            const std::size_t slot = static_cast<std::size_t>(format_.input_channels) + channel;
            auto* destination = static_cast<unsigned char*>(impl_->buffer_infos[slot].buffers[index]);
            const float* source = output_scratch_[channel].data();
            for (std::uint32_t frame = 0; frame < frames; ++frame)
            {
                float_to_asio_sample(output_sample_format_, source[frame], destination + frame * output_bytes);
            }
        }

        if (impl_->post_output)
        {
            ASIOOutputReady();
        }
    }
}
