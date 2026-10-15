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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_rt_safety.cpp
 * @brief Tests that the audio-thread paths of RigGraph perform no heap allocation.
 * @author Mustafa Garip
 */

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>

namespace
{
    class SumNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 1.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    class StereoSourceNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] = 1.0f;
                    context.outputs[1][frame] = -1.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 2};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };

    class MonoPassThroughNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}

            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    context.outputs[0][frame] =
                        context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                }
            }

            SushiDSP::PortLayout ports() const override
            {
                return {1, 1};
            }

            void reset() override {}

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
    };
} // namespace

TEST(Small_RigGraph, ProcessPerformsNoHeapAllocationAcrossManyBlocks)
{
    SushiDSP::RigGraph graph(256, 4);
    graph.add_node(std::make_unique<SumNode>());

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    graph.compile(info);

    std::vector<float> output_buffer(256, 0.0f);
    float* output_channels[1] = {output_buffer.data()};

    SushiDSP::test::AllocationTrap trap;

    for (int block = 0; block < 7000; ++block)
    {
        for (std::uint16_t index = 0; index < 100; ++index)
        {
            graph.push_parameter_change(SushiDSP::ParameterChange{0, index, 0.5f});
        }

        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_RigGraph, StereoProcessPerformsNoHeapAllocation)
{
    // The graph covers every path of the edge adaptation. See tests/foundation/core/test_rig_graph/README.md.
    SushiDSP::RigGraph graph(256, 4);
    const auto first_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto second_source = graph.add_node(std::make_unique<StereoSourceNode>());
    const auto sink = graph.add_node(std::make_unique<MonoPassThroughNode>());
    graph.connect(first_source, sink);
    graph.connect(second_source, sink);

    SushiDSP::PrepareInfo info{48000.0, 256, 2};
    graph.compile(info);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    float* output_channels[2] = {left.data(), right.data()};

    SushiDSP::test::AllocationTrap trap;

    for (int block = 0; block < 2000; ++block)
    {
        trap.arm();

        SushiDSP::ProcessContext context{output_channels, nullptr, 256, 0.0};
        graph.process(context);

        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);
}
