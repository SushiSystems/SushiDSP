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
 * @brief Tests that the audio-thread paths of TubeScreamer perform no heap allocation.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256u;

    /// @brief Runs `blocks` blocks through a prepared node, sweeping Drive up and Tone down
    /// across the run so every block moves a knob.
    void process_sweeping_knobs(SushiDSP::TubeScreamer& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            const float position = static_cast<float>(block) / static_cast<float>(blocks - 1);
            node.set_drive(position);
            node.set_tone(1.0f - position);
            node.process(context);
        }
    }
} // namespace

TEST(Medium_TubeScreamer, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::TubeScreamer node;
    node.set_drive(0.6f);
    node.set_tone(0.4f);
    node.set_level(0.9f);
    node.prepare(SushiDSP::PrepareInfo{48000.0, K_BLOCK, 1});

    const std::vector<float> input = SushiDSP::measure::sine_stimulus(220.0, 48000.0, K_BLOCK, 0.3f);
    std::vector<float> output(K_BLOCK, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};
    SushiDSP::ProcessContext context{output_channels, input_channels, K_BLOCK, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    // A moved knob re-derives the matrices inside process(). See tests/nodes/tubescreamer/test_tube_screamer/README.md.
    process_sweeping_knobs(node, context, 1000);

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
