/**************************************************************************/
/* in_memory_audio_device.hpp                                             */
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
 * @file in_memory_audio_device.hpp
 * @brief Declares the in-memory AudioDevice, which renders blocks when its caller asks, on the caller's thread.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/core/audio_device.hpp>

namespace SushiDSP
{
    /// @brief An AudioDevice held in memory: it starts no thread, and render() runs the blocks.
    ///
    /// It keeps every promise of AudioDevice: each block hands the callback block_size frames, one
    /// buffer per output channel, and the input channels, or null when the format has none. The
    /// input channels carry the levels set_input_level() gave them; the outputs start each block
    /// silent and keep what the last block wrote.
    class InMemoryAudioDevice final : public AudioDevice
    {
        public:
            /// @brief Makes a stopped device of @p format with silent inputs.
            /// @param format The format format() reports and render() follows.
            explicit InMemoryAudioDevice(DeviceFormat format)
                : format_(format),
                  inputs_(format.input_channels, std::vector<float>(format.block_size, 0.0f)),
                  outputs_(format.output_channels, std::vector<float>(format.block_size, 0.0f)),
                  input_pointers_(format.input_channels, nullptr),
                  output_pointers_(format.output_channels, nullptr)
            {
                for (std::uint32_t channel = 0; channel < format.input_channels; ++channel)
                {
                    input_pointers_[channel] = inputs_[channel].data();
                }
                for (std::uint32_t channel = 0; channel < format.output_channels; ++channel)
                {
                    output_pointers_[channel] = outputs_[channel].data();
                }
            }

            /// @brief Returns the format the device was made with.
            DeviceFormat format() const override
            {
                return format_;
            }

            /// @brief Keeps @p callback for render() and marks the device running.
            void start(DeviceCallback callback) override
            {
                callback_ = std::move(callback);
            }

            /// @brief Drops the callback, so render() runs nothing until the next start().
            void stop() override
            {
                callback_ = nullptr;
            }

            /// @brief Returns whether a callback is installed.
            bool is_running() const
            {
                return static_cast<bool>(callback_);
            }

            /// @brief Sets every sample of input channel @p channel to @p level; a channel the format lacks is ignored.
            void set_input_level(std::uint32_t channel, float level)
            {
                if (channel < inputs_.size())
                {
                    inputs_[channel].assign(format_.block_size, level);
                }
            }

            /// @brief Runs @p blocks blocks through the callback, each starting from silent outputs.
            /// @return The blocks run: @p blocks while running, 0 when stopped.
            std::uint32_t render(std::uint32_t blocks)
            {
                if (!callback_)
                {
                    return 0;
                }
                for (std::uint32_t block = 0; block < blocks; ++block)
                {
                    for (std::vector<float>& channel : outputs_)
                    {
                        channel.assign(format_.block_size, 0.0f);
                    }
                    callback_(inputs_.empty() ? nullptr : input_pointers_.data(), output_pointers_.data(),
                              format_.block_size);
                }
                return blocks;
            }

            /// @brief Returns output channel @p channel as the last block left it.
            const std::vector<float>& output(std::uint32_t channel) const
            {
                return outputs_[channel];
            }

        private:
            DeviceFormat format_;
            DeviceCallback callback_;
            std::vector<std::vector<float>> inputs_;
            std::vector<std::vector<float>> outputs_;
            std::vector<const float*> input_pointers_;
            std::vector<float*> output_pointers_;
    };
} // namespace SushiDSP
