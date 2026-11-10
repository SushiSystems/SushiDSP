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
 * @brief Tests that the audio-thread paths of Jcm800 perform no heap allocation.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/signal_measure/render.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>

namespace
{
    /// @brief Runs `blocks` blocks of one context through a prepared node.
    void process_blocks(SushiDSP::AudioNode& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
        {
            node.process(context);
        }
    }
} // namespace

TEST(Medium_Jcm800, ProcessPerformsNoHeapAllocationWithoutCabinet)
{
    // The per-block set_high_shelf() is pure arithmetic. See tests/nodes/jcm800/test_jcm800/README.md.
    SushiDSP::Jcm800 node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    node.set_preamp(0.8f);
    node.set_master(0.0f);
    node.set_bass(0.6f);
    node.set_middle(0.4f);
    node.set_treble(0.7f);
    node.set_presence(0.9f);

    const std::vector<float> input = SushiDSP::measure::sine_stimulus(220.0, 48000.0, 256, 0.3f);
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    process_blocks(node, context, 500);

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
