/**************************************************************************/
/* sdl2_audio_device.cpp                                                  */
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
 * @file sdl2_audio_device.cpp
 * @brief Implements the SDL2-backed audio device.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/device/sdl2_audio_device.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL.h>

namespace SushiDSP
{
    Sdl2AudioDevice::Sdl2AudioDevice(const DeviceFormat& requested_format)
        : format_(requested_format), device_id_(0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            throw std::runtime_error(std::string("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: ") +
                                     SDL_GetError());
        }

        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = static_cast<int>(requested_format.sample_rate);
        desired.format = AUDIO_F32SYS;
        desired.channels = static_cast<Uint8>(requested_format.output_channels);
        desired.samples = static_cast<Uint16>(requested_format.block_size);
        desired.callback = &Sdl2AudioDevice::sdl_callback;
        desired.userdata = this;

        SDL_AudioSpec obtained;
        device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (device_id_ == 0)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            throw std::runtime_error(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
        }

        format_.sample_rate = obtained.freq;
        format_.block_size = obtained.samples;
        format_.input_channels = 0;
        format_.output_channels = obtained.channels;

        server_ = std::make_unique<BlockServer>(format_);
    }

    Sdl2AudioDevice::~Sdl2AudioDevice()
    {
        if (device_id_ != 0)
        {
            SDL_CloseAudioDevice(device_id_);
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    DeviceFormat Sdl2AudioDevice::format() const
    {
        return format_;
    }

    void Sdl2AudioDevice::add_counters_to(RtCounterReporter& reporter) const
    {
        server_->counters().add_to(reporter);
    }

    const DeviceCounters& Sdl2AudioDevice::counters() const noexcept
    {
        return server_->counters();
    }

    void Sdl2AudioDevice::start(DeviceCallback callback)
    {
        server_->set_callback(std::move(callback));
        SDL_PauseAudioDevice(device_id_, 0);
    }

    void Sdl2AudioDevice::stop()
    {
        SDL_PauseAudioDevice(device_id_, 1);

        // SDL_PauseAudioDevice waits out any in-flight callback, so clearing is safe (modules/foundation/device/README.md).
        server_->clear_callback();
    }

    void Sdl2AudioDevice::sdl_callback(void* userdata, unsigned char* stream, int length_bytes)
    {
        auto* self = static_cast<Sdl2AudioDevice*>(userdata);
        const std::uint32_t frames = static_cast<std::uint32_t>(length_bytes) /
                                     (self->format_.output_channels * sizeof(float));
        self->server_->serve(reinterpret_cast<float*>(stream), frames);
    }
} // namespace SushiDSP
