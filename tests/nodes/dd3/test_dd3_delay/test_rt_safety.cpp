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
 * @brief Tests that the audio-thread paths of Dd3Delay perform no heap allocation.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/nodes/dd3/dd3_delay.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Returns whether every sample is a finite number.
    bool all_finite(const std::vector<float>& samples)
    {
        for (const float sample : samples)
        {
            if (!std::isfinite(sample))
            {
                return false;
            }
        }
        return true;
    }
} // namespace

TEST(Medium_Dd3Delay, ProcessPerformsNoHeapAllocation)
{
    // Both MODE changes below move N and pay the std::fill. See tests/nodes/dd3/test_dd3_delay/README.md.
    SushiDSP::Dd3Delay node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, K_BLOCK, 1});
    node.set_time(0.4f);
    node.set_feedback(0.45f);
    node.set_level(0.5f);

    const std::vector<float> input = SushiDSP::measure::sine_stimulus(220.0, 48000.0, K_BLOCK, 0.3f);
    std::vector<float> output(K_BLOCK, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};

    const std::uint16_t mode_index = 3u;
    const float long_mode_value =
        static_cast<float>(static_cast<int>(SushiDSP::dd3::Dd3Mode::Long));

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    node.process(context);
    node.set_parameter_for_audio_thread(mode_index, long_mode_value);
    node.process(context);
    node.set_mode(SushiDSP::dd3::Dd3Mode::Short);
    node.process(context);

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
    EXPECT_TRUE(all_finite(output));
}
