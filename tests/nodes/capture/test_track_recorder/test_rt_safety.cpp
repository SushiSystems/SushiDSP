/**************************************************************************/
/* test_rt_safety.cpp                                                     */
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
 * @file test_rt_safety.cpp
 * @brief Tests that TrackRecorder::write allocates nothing on the audio thread.
 * @author Mustafa Garip
 */

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/capture/track_recorder.hpp>

namespace
{
    /// @brief Owns a fresh folder under the temporary directory and removes it on exit.
    class TempFolder
    {
        public:
            TempFolder()
                : root_(std::filesystem::temp_directory_path() / "sushidsp_rt_safety_recorder")
            {
                std::filesystem::remove_all(root_);
                std::filesystem::create_directories(root_);
            }

            ~TempFolder()
            {
                std::filesystem::remove_all(root_);
            }

            TempFolder(const TempFolder&) = delete;
            TempFolder& operator=(const TempFolder&) = delete;

            const std::filesystem::path& path() const
            {
                return root_;
            }

        private:
            std::filesystem::path root_;
    };

    /// @brief Writes the same block to the recorder several times with the trap armed.
    void write_blocks_trapped(SushiDSP::TrackRecorder& recorder, const float* const* channels,
                              int blocks, SushiDSP::test::AllocationTrap& trap)
    {
        trap.arm();
        for (int block = 0; block < blocks; ++block)
        {
            recorder.write(channels, 256u);
        }
        trap.disarm();
    }
} // namespace

TEST(Medium_TrackRecorder, WritePerformsNoHeapAllocation)
{
    // The trap sees the ring copy and the drop counter. See tests/nodes/capture/test_track_recorder/README.md.
    const TempFolder folder;
    SushiDSP::TrackRecorder recorder(2u, 48000.0, 65536);
    ASSERT_TRUE(recorder.arm((folder.path() / "rt_safety.wav").string()));
    std::vector<float> left(256, 0.25f);
    std::vector<float> right(256, -0.25f);
    const float* channels[2] = {left.data(), right.data()};
    ASSERT_TRUE(recorder.write(channels, 256u));
    SushiDSP::test::AllocationTrap trap;

    write_blocks_trapped(recorder, channels, 200, trap);

    EXPECT_EQ(trap.count(), 0);
    recorder.stop();
}
