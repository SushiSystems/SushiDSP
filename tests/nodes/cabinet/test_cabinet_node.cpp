/**************************************************************************/
/* test_cabinet_node.cpp                                                  */
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
 * @file test_cabinet_node.cpp
 * @brief Tests CabinetNode with and without an impulse response installed.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/cabinet/cabinet_node.hpp>
#include <SushiDSP/foundation/io/ir.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    std::vector<float> make_identity_impulse_response()
    {
        return std::vector<float>{1.0f};
    }
} // namespace

TEST(Small_CabinetNode, CabinetNodeIsAPassthroughWithNoCabinetInstalled)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);

    std::vector<float> input(256);
    for (std::uint32_t i = 0; i < input.size(); ++i)
    {
        input[i] = std::sin(static_cast<float>(i) * 0.1f);
    }
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(input.size()),
                                     0.0};

    node.process(context);

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Small_CabinetNode, CabinetNodeInstallsAndAppliesAnImpulseResponse)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);

    EXPECT_FALSE(node.has_cabinet());
    EXPECT_TRUE(node.set_cabinet(SushiDSP::Ir{make_identity_impulse_response(), 48000.0}));
    EXPECT_TRUE(node.has_cabinet());
    node.reclaim_cabinets();

    std::vector<float> input(256, 1.0f);
    std::vector<float> output(256, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(input.size()),
                                     0.0};

    node.process(context);

    // An identity IR makes the cabinet a one-partition delay. See tests/nodes/cabinet/README.md.
    const std::uint32_t latency = node.latency_samples();
    for (std::uint32_t i = latency; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(output[i], 1.0f);
    }
}

TEST(Small_CabinetNode, CabinetNodeSurvivesVariableBlockSizes)
{
    // An identity IR must survive any host block size. See tests/nodes/cabinet/README.md.
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 512, 1};
    node.prepare(info);
    ASSERT_TRUE(node.set_cabinet(SushiDSP::Ir{make_identity_impulse_response(), 48000.0}));
    node.reclaim_cabinets();

    std::vector<float> input(512);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(0.03f * static_cast<float>(i));
    }
    const float* input_channels[1] = {input.data()};

    std::vector<float> stream_in, stream_out;
    const std::uint32_t chunks[] = {100, 37, 512, 1, 300};
    for (std::uint32_t frames : chunks)
    {
        std::vector<float> output(frames, 0.0f);
        float* output_channels[1] = {output.data()};
        SushiDSP::ProcessContext context{output_channels, input_channels, frames, 0.0};
        node.process(context);
        stream_out.insert(stream_out.end(), output.begin(), output.end());
        stream_in.insert(stream_in.end(), input.begin(), input.begin() + frames);
    }

    const std::uint32_t latency = node.latency_samples();
    EXPECT_EQ(latency, 128u);
    for (std::size_t n = latency; n < stream_out.size(); ++n)
    {
        EXPECT_NEAR(stream_out[n], stream_in[n - latency], 1e-5f);
    }
}

TEST(Small_CabinetNode, CabinetNodeRejectsAnImpulseResponseAtTheWrongSampleRate)
{
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{make_identity_impulse_response(), 44100.0}));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Small_CabinetNode, CabinetNodeRejectsAnImpulseResponseBeforePrepare)
{
    SushiDSP::CabinetNode node;

    EXPECT_FALSE(node.set_cabinet(SushiDSP::Ir{make_identity_impulse_response(), 48000.0}));
    EXPECT_FALSE(node.has_cabinet());
}

TEST(Small_CabinetNode, CabinetNodeDescriptorMatchesTypeId)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::cabinet_descriptor();
    EXPECT_STREQ(descriptor.id, "sushidsp.cabinet");
    EXPECT_EQ(descriptor.ports.input_channels, 1u);
    EXPECT_EQ(descriptor.ports.output_channels, 1u);
    EXPECT_EQ(descriptor.parameter_count, 0u);

    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    auto node = descriptor.make(info);
    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), "sushidsp.cabinet");
}

TEST(Small_CabinetNode, ProcessPerformsNoHeapAllocationWithCabinet)
{
    // 500 blocks wrap the spectrum history at least once. See tests/nodes/cabinet/README.md.
    std::vector<float> impulse_response(200, 0.0f);
    impulse_response[0] = 1.0f;
    for (std::size_t i = 1; i < impulse_response.size(); ++i)
    {
        impulse_response[i] = 0.5f * impulse_response[i - 1];
    }

    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);
    ASSERT_TRUE(node.set_cabinet(SushiDSP::Ir{impulse_response, 48000.0}));

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    SushiDSP::test::AllocationTrap trap;
    trap.arm();

    for (int block = 0; block < 500; ++block)
    {
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
    }

    trap.disarm();

    EXPECT_EQ(trap.count(), 0);
}

TEST(Small_CabinetNode, SetCabinetSwapsWithoutAllocatingOnTheAudioThread)
{
    // The trap is armed around the blocks, not set_cabinet(). See tests/nodes/cabinet/README.md.
    SushiDSP::CabinetNode node;
    SushiDSP::PrepareInfo info{48000.0, 256, 1};
    node.prepare(info);

    SushiDSP::Ir impulse_response;
    impulse_response.sample_rate = 48000.0;
    impulse_response.samples.assign(2048, 0.0f);
    impulse_response.samples[0] = 1.0f;

    std::vector<float> input(256);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.2f * std::sin(2.0f * static_cast<float>(M_PI) * 220.0f * i / 48000.0f);
    }
    std::vector<float> output(256, 0.0f);
    const float* input_channels[1] = {input.data()};
    float* output_channels[1] = {output.data()};

    SushiDSP::test::AllocationTrap trap;

    for (int block = 0; block < 4; ++block)
    {
        impulse_response.samples[0] = 1.0f - 0.1f * static_cast<float>(block);
        ASSERT_TRUE(node.set_cabinet(impulse_response));
        ASSERT_TRUE(node.has_cabinet());

        trap.arm();
        SushiDSP::ProcessContext context{output_channels, input_channels, 256, 0.0};
        node.process(context);
        trap.disarm();
    }

    EXPECT_EQ(trap.count(), 0);

    node.reclaim_cabinets();
}
