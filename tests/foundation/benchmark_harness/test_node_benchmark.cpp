/**************************************************************************/
/* test_node_benchmark.cpp                                                */
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
 * @file test_node_benchmark.cpp
 * @brief Tests that measure_node() prepares, configures and runs the node it times.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>

namespace
{
    /// @brief Declares a node that records what the harness did to it.
    class RecordingNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                prepared_block = info.max_block;
            }

            void process(SushiDSP::ProcessContext& context) override
            {
                ++process_calls;
                frames = context.frames;
                for (std::uint32_t i = 0; i < context.frames; ++i)
                    context.outputs[0][i] = context.inputs[0][i];
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }

            void set_parameter_for_control_thread(const char*, float) override
            {

            }

            void reset() override
            {

            }

            static inline std::uint32_t prepared_block = 0;
            static inline std::uint32_t frames = 0;
            static inline int process_calls = 0;
    };
} // namespace

TEST(Medium_NodeBenchmark, NodeBenchmarkPreparesConfiguresAndProcessesOneBlockPerRepetition)
{
    bool configured_after_prepare = false;
    const SushiDSP::bench::Result result = SushiDSP::bench::measure_node<RecordingNode>(
        "recording",
        [&](RecordingNode&)
        { configured_after_prepare = RecordingNode::prepared_block == SushiDSP::bench::K_BLOCK_SIZE; });

    EXPECT_TRUE(configured_after_prepare);
    EXPECT_EQ(RecordingNode::process_calls, 2020);
    EXPECT_EQ(RecordingNode::frames, SushiDSP::bench::K_BLOCK_SIZE);
    EXPECT_STREQ(result.name, "recording");
}
