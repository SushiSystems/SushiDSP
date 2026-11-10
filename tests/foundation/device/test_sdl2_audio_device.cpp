/**************************************************************************/
/* test_sdl2_audio_device.cpp                                             */
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
 * @file test_sdl2_audio_device.cpp
 * @brief Tests that a block from SDL2's dummy audio driver reaches the device's callback.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/device/sdl2_audio_device.hpp>

#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <mutex>

namespace
{
    /// @brief Points SDL2 at its dummy driver, which has no hardware and paces blocks itself.
    void use_dummy_driver()
    {
#if defined(_WIN32)
        _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
        setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
    }

    /// @brief Records the frame count of the first block the callback sees and wakes the waiting test.
    class FirstBlock
    {
        public:
            /// @brief Stores @p frames the first time it is called.
            void record(std::uint32_t frames)
            {
                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    if (!seen_)
                    {
                        frames_ = frames;
                        seen_ = true;
                    }
                }
                arrived_.notify_all();
            }

            /// @brief Blocks until record() has run and returns the frames it stored.
            std::uint32_t wait()
            {
                std::unique_lock<std::mutex> lock(mutex_);
                arrived_.wait(lock, [this] { return seen_; });
                return frames_;
            }

        private:
            std::mutex mutex_;
            std::condition_variable arrived_;
            std::uint32_t frames_ = 0;
            bool seen_ = false;
    };
} // namespace

TEST(Medium_Sdl2AudioDevice, ABlockFromTheDriverReachesTheCallbackWithTheBlockSize)
{
    use_dummy_driver();
    SushiDSP::Sdl2AudioDevice device(SushiDSP::DeviceFormat{48000.0, 256, 0, 2});
    FirstBlock first;

    device.start([&](const float* const*, float* const*, std::uint32_t frames) { first.record(frames); });
    const std::uint32_t frames = first.wait();
    device.stop();

    EXPECT_EQ(frames, device.format().block_size);
}
