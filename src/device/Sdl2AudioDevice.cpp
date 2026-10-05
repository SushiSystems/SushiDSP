/****************************************************************/
/* Sdl2AudioDevice.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "SushiDSP/device/Sdl2AudioDevice.hpp"

#include "SushiDSP/core/DenormalGuard.hpp"

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

        // A one-channel device hands its own buffer to the callback, so only a wider
        // device needs somewhere deinterleaved to render into.
        if (format_.output_channels > 1)
        {
            deinterleaved_output_.assign(
                static_cast<std::size_t>(format_.block_size) * format_.output_channels, 0.0f);
            output_channel_pointers_.resize(format_.output_channels);
            for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
            {
                output_channel_pointers_[channel] =
                    deinterleaved_output_.data() + static_cast<std::size_t>(channel) * format_.block_size;
            }
        }
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

    void Sdl2AudioDevice::interleave(const float* const* channels, std::uint32_t channel_count,
                                     std::uint32_t frames, float* destination)
    {
        for (std::uint32_t frame = 0; frame < frames; ++frame)
        {
            for (std::uint32_t channel = 0; channel < channel_count; ++channel)
            {
                destination[static_cast<std::size_t>(frame) * channel_count + channel] = channels[channel][frame];
            }
        }
    }

    void Sdl2AudioDevice::sdl_callback(void* userdata, unsigned char* stream, int length_bytes)
    {
        DenormalGuard denormal_guard;
        auto* self = static_cast<Sdl2AudioDevice*>(userdata);
        auto* samples = reinterpret_cast<float*>(stream);
        const std::uint32_t frames = static_cast<std::uint32_t>(length_bytes) / (self->format_.output_channels * sizeof(float));

        if (!self->callback_)
        {
            std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
            return;
        }

        if (self->format_.output_channels == 1)
        {
            float* channel_pointers[1] = { samples };
            self->callback_(nullptr, channel_pointers, frames);
            return;
        }

        // The deinterleaved scratch holds exactly one block, so a longer request cannot be served.
        if (frames > self->format_.block_size)
        {
            std::memset(stream, 0, static_cast<std::size_t>(length_bytes));
            return;
        }

        self->callback_(nullptr, self->output_channel_pointers_.data(), frames);
        interleave(self->output_channel_pointers_.data(), self->format_.output_channels, frames, samples);
    }
}
