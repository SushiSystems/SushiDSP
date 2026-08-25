/**************************************************************************/
/* Sdl2AudioDevice.cpp                                                    */
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

#include "SushiDSP/device/Sdl2AudioDevice.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL.h>

namespace SushiDSP
{
    Sdl2AudioDevice::Sdl2AudioDevice(const DeviceFormat& requested_format)
        : format_(requested_format)
        , device_id_(0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            throw std::runtime_error(std::string("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: ") + SDL_GetError());
        }

        SDL_AudioSpec desired;
        SDL_zero(desired);
        desired.freq = static_cast<int>(requested_format.sample_rate);
        desired.format = AUDIO_F32SYS;
        desired.channels = static_cast<Uint8>(requested_format.channels);
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
        format_.channels = obtained.channels;
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

    void Sdl2AudioDevice::start(DeviceCallback callback)
    {
        callback_ = std::move(callback);
        SDL_PauseAudioDevice(device_id_, 0);
    }

    void Sdl2AudioDevice::stop()
    {
        SDL_PauseAudioDevice(device_id_, 1);

        // Once stopped, the device must not keep a reference to whatever the
        // caller's callback captured; SDL_PauseAudioDevice waits for any
        // in-flight callback to finish, so clearing here is safe.
        callback_ = {};
    }

    /// @brief SDL2's audio thread entry point. Only mono is deinterleaved in D1; a
    /// multi-channel device outputs silence rather than crashing until a later task
    /// needs real interleave/deinterleave.
    void Sdl2AudioDevice::sdl_callback(void* userdata, unsigned char* stream, int length_bytes)
    {
        auto* self = static_cast<Sdl2AudioDevice*>(userdata);
        auto* samples = reinterpret_cast<float*>(stream);
        const std::uint32_t frames = static_cast<std::uint32_t>(length_bytes) / (self->format_.channels * sizeof(float));

        if (!self->callback_)
        {
            std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
            return;
        }

        if (self->format_.channels == 1)
        {
            float* channel_pointers[1] = { samples };
            self->callback_(channel_pointers, frames);
            return;
        }

        std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
    }
}
