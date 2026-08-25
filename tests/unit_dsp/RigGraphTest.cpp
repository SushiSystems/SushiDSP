// RigGraphTest.cpp
//
// Copyright 2026 SushiDSP Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/core/NodeDescriptor.hpp"
#include "SushiDSP/core/RigGraph.hpp"

namespace
{
    class GainNode : public SushiDSP::IAudioNode
    {
    public:
        explicit GainNode(float gain) : gain_(gain) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = context.inputs[0][frame] * gain_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 1, 1 }; }

        void reset() override {}

    private:
        float gain_;
    };

    class ConstantSourceNode : public SushiDSP::IAudioNode
    {
    public:
        explicit ConstantSourceNode(float value) : value_(value) {}

        void prepare(const SushiDSP::PrepareInfo&) override {}

        void process(SushiDSP::ProcessContext& context) override
        {
            for (std::uint32_t frame = 0; frame < context.frames; ++frame)
            {
                context.outputs[0][frame] = value_;
            }
        }

        SushiDSP::PortLayout ports() const override { return { 0, 1 }; }

        void reset() override {}

    private:
        float value_;
    };
}

TEST(UnitDsp, PortLayoutIsPlainOldData)
{
    SushiDSP::PortLayout layout{ 1, 2 };
    EXPECT_EQ(layout.input_channels, 1u);
    EXPECT_EQ(layout.output_channels, 2u);
}

TEST(UnitDsp, RigGraphProcessesTwoNodeChainInOrder)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(2.0f));
    auto gain_handle    = graph.add_node(std::make_unique<GainNode>(3.0f));
    graph.connect(source_handle, gain_handle);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };

    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 6.0f);
    }
}

TEST(UnitDsp, RigGraphRecompileAfterAddingNodeProducesUpdatedOutput)
{
    SushiDSP::RigGraph graph(256, 4);
    auto source_handle = graph.add_node(std::make_unique<ConstantSourceNode>(1.0f));
    auto gain_handle_a  = graph.add_node(std::make_unique<GainNode>(2.0f));
    graph.connect(source_handle, gain_handle_a);

    SushiDSP::PrepareInfo info{ 48000.0, 256, 1 };
    graph.compile(info);

    auto gain_handle_b = graph.add_node(std::make_unique<GainNode>(5.0f));
    graph.connect(gain_handle_a, gain_handle_b);
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = { output_buffer.data() };
    SushiDSP::ProcessContext context{ output_channels, nullptr, 256, 0.0 };
    graph.process(context);

    for (float sample : output_buffer)
    {
        EXPECT_FLOAT_EQ(sample, 10.0f);
    }
}
