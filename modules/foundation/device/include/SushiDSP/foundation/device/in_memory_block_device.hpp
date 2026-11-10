/**************************************************************************/
/* in_memory_block_device.hpp                                             */
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
 * @file in_memory_block_device.hpp
 * @brief Declares the in-memory AudioDevice that serves blocks on the caller's thread through a BlockServer.
 * @author Mustafa Garip
 * @see modules/foundation/device/README.md
 */

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <SushiDSP/foundation/core/audio_device.hpp>
#include <SushiDSP/foundation/device/block_clock.hpp>
#include <SushiDSP/foundation/device/block_server.hpp>

namespace SushiDSP
{
    /// @brief An AudioDevice with no driver and no thread: serve() answers a block when the caller asks.
    ///
    /// It keeps every promise of AudioDevice: start() installs the callback, stop() removes it, and
    /// add_counters_to() offers the counters that serve() raises. Each block is served by a
    /// BlockServer, the same step the SDL2 device runs on its audio thread.
    class InMemoryBlockDevice final : public AudioDevice
    {
        public:
            /// @brief Builds a device of @p format that reads the steady clock.
            explicit InMemoryBlockDevice(const DeviceFormat& format) : format_(format), server_(format)
            {
            }

            /// @brief Builds a device of @p format that reads @p clock, which must outlive it.
            InMemoryBlockDevice(const DeviceFormat& format, const BlockClock& clock)
                : format_(format), server_(format, clock)
            {
            }

            /// @brief Returns the format the device was made with.
            DeviceFormat format() const override
            {
                return format_;
            }

            /// @brief Installs @p callback for the blocks serve() answers.
            void start(DeviceCallback callback) override
            {
                server_.set_callback(std::move(callback));
            }

            /// @brief Removes the callback.
            void stop() override
            {
                server_.clear_callback();
            }

            /// @brief Adds the counters serve() raises to @p reporter.
            void add_counters_to(RtCounterReporter& reporter) const override
            {
                server_.counters().add_to(reporter);
            }

            /// @brief Returns what serve() counted.
            [[nodiscard]] const DeviceCounters& counters() const noexcept
            {
                return server_.counters();
            }

            /// @brief Serves one block of @p frames frames into a buffer that held stale samples.
            /// @return The interleaved block.
            const std::vector<float>& serve(std::uint32_t frames)
            {
                stream_.assign(static_cast<std::size_t>(frames) * format_.output_channels, K_STALE_SAMPLE);
                server_.serve(stream_.data(), frames);
                return stream_;
            }

            /// @brief The value the buffer holds before each block, so a block that writes nothing shows.
            static constexpr float K_STALE_SAMPLE = 1.0f;

        private:
            /// @brief The format the device was made with.
            DeviceFormat format_;

            /// @brief Answers each block.
            BlockServer server_;

            /// @brief The last block served.
            std::vector<float> stream_;
    };
} // namespace SushiDSP
