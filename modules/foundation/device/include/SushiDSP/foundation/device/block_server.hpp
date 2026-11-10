/**************************************************************************/
/* block_server.hpp                                                       */
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
 * @file block_server.hpp
 * @brief Declares the step that answers one block of a device: the callback, its dropouts and its counters.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/core/denormal_guard.hpp>
#include <SushiDSP/foundation/device/block_monitor.hpp>
#include <SushiDSP/foundation/device/device_counters.hpp>

namespace SushiDSP
{
    /// @brief Answers each block a device asks for, on the device's audio thread.
    ///
    /// A device calls serve() with its interleaved output buffer. serve() times the block, runs the
    /// installed callback, interleaves a multi-channel result and raises the counters of
    /// DeviceCounters. It holds no driver type, so a test calls it with a plain buffer.
    class BlockServer final
    {
        public:
            /// @brief Builds a server for @p format that reads the steady clock.
            explicit BlockServer(const DeviceFormat& format) : format_(format), monitor_(counters_)
            {
                prepare_scratch();
            }

            /// @brief Builds a server for @p format that reads @p clock, which must outlive it.
            BlockServer(const DeviceFormat& format, const BlockClock& clock)
                : format_(format), monitor_(counters_, clock)
            {
                prepare_scratch();
            }

            BlockServer(const BlockServer&) = delete;
            BlockServer& operator=(const BlockServer&) = delete;
            BlockServer(BlockServer&&) = delete;
            BlockServer& operator=(BlockServer&&) = delete;

            /// @brief Installs the callback the next blocks run.
            void set_callback(DeviceCallback callback)
            {
                callback_ = std::move(callback);
            }

            /// @brief Removes the callback; the next blocks are answered with silence.
            void clear_callback()
            {
                callback_ = {};
            }

            /// @brief Returns what the audio thread counted; read it on the control thread.
            [[nodiscard]] const DeviceCounters& counters() const noexcept
            {
                return counters_;
            }

            /// @brief Answers one block into @p stream.
            /// @param stream The interleaved output buffer, @p frames times the output channel count samples.
            /// @param frames The frames the device asks for.
            void serve(float* stream, std::uint32_t frames)
            {
                DenormalGuard denormal_guard;
                const BlockScope block_scope(monitor_, frames, format_.sample_rate);

                if (!callback_)
                {
                    counters_.no_callback.increment();
                    silence(stream, frames);
                    return;
                }

                if (format_.output_channels == 1)
                {
                    float* channel_pointers[1] = {stream};
                    callback_(nullptr, channel_pointers, frames);
                    return;
                }

                // The deinterleaved scratch holds exactly one block, so a longer request cannot be served.
                if (frames > format_.block_size)
                {
                    counters_.oversize_block.note(frames);
                    silence(stream, frames);
                    return;
                }

                callback_(nullptr, output_channel_pointers_.data(), frames);
                interleave(stream, frames);
            }

        private:
            /// @brief Sizes the scratch a multi-channel device deinterleaves into.
            void prepare_scratch()
            {
                if (format_.output_channels <= 1)
                {
                    return;
                }
                deinterleaved_output_.assign(static_cast<std::size_t>(format_.block_size) * format_.output_channels,
                                             0.0f);
                output_channel_pointers_.resize(format_.output_channels);
                for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
                {
                    output_channel_pointers_[channel] =
                        deinterleaved_output_.data() + static_cast<std::size_t>(channel) * format_.block_size;
                }
            }

            /// @brief Writes silence over a block of @p frames frames.
            void silence(float* stream, std::uint32_t frames) const
            {
                const std::size_t samples = static_cast<std::size_t>(frames) * format_.output_channels;
                for (std::size_t index = 0; index < samples; ++index)
                {
                    stream[index] = 0.0f;
                }
            }

            /// @brief Interleaves the scratch channels into @p destination frame by frame.
            void interleave(float* destination, std::uint32_t frames) const
            {
                for (std::uint32_t frame = 0; frame < frames; ++frame)
                {
                    for (std::uint32_t channel = 0; channel < format_.output_channels; ++channel)
                    {
                        destination[static_cast<std::size_t>(frame) * format_.output_channels + channel] =
                            output_channel_pointers_[channel][frame];
                    }
                }
            }

            /// @brief The format blocks are served at.
            DeviceFormat format_;

            /// @brief The caller's per-block callback, empty while none is installed.
            DeviceCallback callback_;

            /// @brief Counts the blocks the callback could not serve and the dropouts.
            DeviceCounters counters_;

            /// @brief Marks each block on the timeline and counts the dropouts.
            BlockMonitor monitor_;

            /// @brief One deinterleaved output channel after another, block_size frames each; empty for one channel.
            std::vector<float> deinterleaved_output_;

            /// @brief Points at each channel's span of deinterleaved_output_, in channel order.
            std::vector<float*> output_channel_pointers_;
    };
} // namespace SushiDSP
