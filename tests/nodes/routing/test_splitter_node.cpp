/**************************************************************************/
/* test_splitter_node.cpp                                                 */
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
 * @file test_splitter_node.cpp
 * @brief Tests that SplitterNode copies its input to its output without allocating.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/routing/splitter_node.hpp>

#include <cstddef>
#include <vector>

namespace
{
    /// @brief Holds the frames of one block in the tests below.
    constexpr std::size_t K_FRAMES = 64;

    /// @brief Holds the blocks the allocation test processes.
    constexpr int K_TRAP_BLOCKS = 100;

    /// @brief Holds the value the copy test feeds every frame.
    constexpr float K_INPUT_LEVEL = 3.5f;

    /// @brief Processes @p blocks blocks of @p context on @p node.
    void process_blocks(SushiDSP::SplitterNode& node, SushiDSP::ProcessContext& context, int blocks)
    {
        for (int block = 0; block < blocks; ++block)
            node.process(context);
    }

    /// @brief Expects every sample of @p samples to equal @p expected.
    void expect_every_sample(const std::vector<float>& samples, float expected)
    {
        for (const float sample : samples)
            EXPECT_FLOAT_EQ(sample, expected);
    }
} // namespace

TEST(Small_SplitterNode, SplitterNodeCopiesInputToOutput)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, K_FRAMES, 1});
    std::vector<float> input(K_FRAMES, K_INPUT_LEVEL);
    std::vector<float> output(K_FRAMES, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, K_FRAMES, 0.0};

    node.process(context);

    expect_every_sample(output, K_INPUT_LEVEL);
}

TEST(Medium_SplitterNode, ProcessPerformsNoHeapAllocation)
{
    SushiDSP::SplitterNode node;
    node.prepare(SushiDSP::PrepareInfo{48000.0, 256, 1});
    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 256, 0.0};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();
    process_blocks(node, context, K_TRAP_BLOCKS);
    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}
