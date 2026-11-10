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
 * @file test_cabinet_node.cpp
 * @brief Tests CabinetNode with and without an impulse response installed.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/cabinet/cabinet_node.hpp>
#include <SushiDSP/foundation/io/ir.hpp>
#include <SushiDSP/nodes/cabinet/in_memory_cabinet_log.hpp>

#include "support/tolerance.hpp"

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr float K_TWO_PI = 6.28318531f;

    SushiDSP::PrepareInfo prepare_info(std::uint32_t max_block)
    {
        return SushiDSP::PrepareInfo{K_SAMPLE_RATE, max_block, 1};
    }

    SushiDSP::Ir make_identity_ir(double sample_rate)
    {
        return SushiDSP::Ir{std::vector<float>{1.0f}, sample_rate};
    }

    /// Builds an impulse response of one at index 0 whose later taps each halve the one before.
    SushiDSP::Ir make_halving_ir(std::size_t length)
    {
        SushiDSP::Ir impulse_response;
        impulse_response.sample_rate = K_SAMPLE_RATE;
        impulse_response.samples.assign(length, 0.0f);
        impulse_response.samples[0] = 1.0f;
        for (std::size_t i = 1; i < length; ++i)
        {
            impulse_response.samples[i] = 0.5f * impulse_response.samples[i - 1];
        }
        return impulse_response;
    }

    /// Builds a sine of the given amplitude and angular step per sample.
    std::vector<float> make_sine(std::size_t frames, float amplitude, float radians_per_sample)
    {
        std::vector<float> samples(frames);
        for (std::size_t i = 0; i < frames; ++i)
        {
            samples[i] = amplitude * std::sin(radians_per_sample * static_cast<float>(i));
        }
        return samples;
    }

    float sum_of_magnitudes(const std::vector<float>& samples)
    {
        float total = 0.0f;
        for (float sample : samples)
        {
            total += std::fabs(sample);
        }
        return total;
    }

    /// Runs one block of the whole input through the node and returns the output.
    std::vector<float> process_block(SushiDSP::CabinetNode& node, const std::vector<float>& input)
    {
        std::vector<float> output(input.size(), 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(input.size()),
                                         0.0};
        node.process(context);
        return output;
    }

    /// The input and output streams of a run that hands the node blocks of different sizes.
    struct ChunkedRun
    {
        std::vector<float> input_stream;
        std::vector<float> output_stream;
    };

    /// Feeds the node one block per entry of chunks, each block the leading frames of input.
    ChunkedRun process_in_chunks(SushiDSP::CabinetNode& node, const std::vector<float>& input,
                                 const std::vector<std::uint32_t>& chunks)
    {
        ChunkedRun run;
        const float* input_channels[1] = {input.data()};
        for (std::uint32_t frames : chunks)
        {
            std::vector<float> output(frames, 0.0f);
            float* output_channels[1] = {output.data()};
            SushiDSP::ProcessContext context{output_channels, input_channels, frames, 0.0};
            node.process(context);
            run.output_stream.insert(run.output_stream.end(), output.begin(), output.end());
            run.input_stream.insert(run.input_stream.end(), input.begin(), input.begin() + frames);
        }
        return run;
    }

    /// Runs the same block through the node several times with the allocation trap armed.
    void process_blocks_trapped(SushiDSP::CabinetNode& node, const std::vector<float>& input,
                                std::vector<float>& output, int blocks,
                                SushiDSP::test::AllocationTrap& trap)
    {
        const float* input_channels[1] = {input.data()};
        float* output_channels[1] = {output.data()};
        trap.arm();
        for (int block = 0; block < blocks; ++block)
        {
            SushiDSP::ProcessContext context{output_channels, input_channels,
                                             static_cast<std::uint32_t>(input.size()), 0.0};
            node.process(context);
        }
        trap.disarm();
    }

    /// Publishes a new first tap, then processes one trapped block, for each block asked.
    /// Returns how many publishes the node refused.
    int publish_and_process_trapped(SushiDSP::CabinetNode& node, SushiDSP::Ir& impulse_response,
                                    const std::vector<float>& input, std::vector<float>& output,
                                    int blocks, SushiDSP::test::AllocationTrap& trap)
    {
        int refused = 0;
        for (int block = 0; block < blocks; ++block)
        {
            impulse_response.samples[0] = 1.0f - 0.1f * static_cast<float>(block);
            if (node.set_cabinet(impulse_response) != SushiDSP::CabinetStatus::Installed)
            {
                ++refused;
            }
            process_blocks_trapped(node, input, output, 1, trap);
        }
        return refused;
    }

    /// Expects every output sample to be the input sample with no rounding at all.
    void expect_samples_equal(const std::vector<float>& actual, const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(actual[i], expected[i], 0.0)))
                << "sample " << i;
        }
    }

    /// Expects output[n] to be input[n - latency] from the first sample that has seen input.
    /// An identity impulse response makes the cabinet a delay of one partition; its FFT of
    /// 2 * latency points bounds the rounding by n * u * (sum of the input magnitudes).
    void expect_identity_delay(const std::vector<float>& output, const std::vector<float>& input,
                               std::uint32_t latency)
    {
        ASSERT_EQ(output.size(), input.size());
        const double magnitude = sum_of_magnitudes(input);
        const std::size_t fft_length = 2u * latency;
        for (std::size_t n = latency; n < output.size(); ++n)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_sum_error<float>(
                output[n], input[n - latency], magnitude, fft_length)))
                << "sample " << n;
        }
    }
} // namespace

TEST(Small_CabinetNode, CabinetNodeIsAPassthroughWithNoCabinetInstalled)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));
    const std::vector<float> input = make_sine(256, 1.0f, 0.1f);

    const std::vector<float> output = process_block(node, input);

    expect_samples_equal(output, input);
}

TEST(Small_CabinetNode, SetCabinetInstallsAnImpulseResponseOnAPreparedNode)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));

    EXPECT_EQ(node.set_cabinet(make_identity_ir(K_SAMPLE_RATE)), SushiDSP::CabinetStatus::Installed);
    EXPECT_TRUE(node.has_cabinet());
}

TEST(Small_CabinetNode, CabinetNodeWithAnIdentityImpulseResponseDelaysByItsLatency)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));
    ASSERT_EQ(node.set_cabinet(make_identity_ir(K_SAMPLE_RATE)), SushiDSP::CabinetStatus::Installed);
    node.reclaim_cabinets();
    const std::vector<float> input(256, 1.0f);

    const std::vector<float> output = process_block(node, input);

    expect_identity_delay(output, input, node.latency_samples());
}

TEST(Small_CabinetNode, CabinetNodeSurvivesVariableBlockSizes)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(512));
    ASSERT_EQ(node.set_cabinet(make_identity_ir(K_SAMPLE_RATE)), SushiDSP::CabinetStatus::Installed);
    node.reclaim_cabinets();
    const std::vector<float> input = make_sine(512, 0.2f, 0.03f);

    const ChunkedRun run = process_in_chunks(node, input, {100, 37, 512, 1, 300});

    EXPECT_EQ(node.latency_samples(), 128u);
    expect_identity_delay(run.output_stream, run.input_stream, node.latency_samples());
}

TEST(Small_CabinetNode, CabinetDescriptorMakesANodeOfItsOwnTypeId)
{
    const SushiDSP::NodeDescriptor& descriptor = SushiDSP::cabinet_descriptor();

    auto node = descriptor.make(prepare_info(256));

    ASSERT_NE(node, nullptr);
    EXPECT_STREQ(node->type_id(), descriptor.id);
}

TEST(Medium_CabinetNode, ProcessPerformsNoHeapAllocationWithCabinet)
{
    // 500 blocks wrap the spectrum history at least once. See tests/nodes/cabinet/README.md.
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));
    ASSERT_EQ(node.set_cabinet(make_halving_ir(200)), SushiDSP::CabinetStatus::Installed);
    const std::vector<float> input = make_sine(256, 0.2f, K_TWO_PI * 220.0f / 48000.0f);
    std::vector<float> output(256, 0.0f);
    SushiDSP::test::AllocationTrap trap;

    process_blocks_trapped(node, input, output, 500, trap);

    EXPECT_EQ(trap.count(), 0);
}

TEST(Medium_CabinetNode, SetCabinetSwapsWithoutAllocatingOnTheAudioThread)
{
    // The trap is armed around the blocks, not set_cabinet(). See tests/nodes/cabinet/README.md.
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));
    SushiDSP::Ir impulse_response = make_identity_ir(K_SAMPLE_RATE);
    impulse_response.samples.assign(2048, 0.0f);
    const std::vector<float> input = make_sine(256, 0.2f, K_TWO_PI * 220.0f / 48000.0f);
    std::vector<float> output(256, 0.0f);
    SushiDSP::test::AllocationTrap trap;

    const int refused = publish_and_process_trapped(node, impulse_response, input, output, 4, trap);

    EXPECT_EQ(refused, 0);
    EXPECT_EQ(trap.count(), 0);
    node.reclaim_cabinets();
}

TEST(Small_CabinetNodeStatus, AnEmptyImpulseResponseIsRefusedWithItsOwnStatus)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));

    EXPECT_EQ(node.set_cabinet(SushiDSP::Ir{}), SushiDSP::CabinetStatus::EmptyImpulseResponse);
}

TEST(Small_CabinetNodeStatus, AnEmptyImpulseResponseReportsOneRefusalForItsCause)
{
    SushiDSP::InMemoryCabinetLog log;
    SushiDSP::CabinetNode node(log);
    node.prepare(prepare_info(256));

    node.set_cabinet(SushiDSP::Ir{});

    ASSERT_EQ(log.refusals().size(), 1u);
    EXPECT_EQ(log.refusals()[0].cause, SushiDSP::CabinetRefusalCause::EmptyImpulseResponse);
}

TEST(Small_CabinetNodeStatus, ARateMismatchIsRefusedWithItsOwnStatus)
{
    SushiDSP::CabinetNode node;
    node.prepare(prepare_info(256));

    EXPECT_EQ(node.set_cabinet(make_identity_ir(44100.0)), SushiDSP::CabinetStatus::SampleRateMismatch);
}

TEST(Small_CabinetNodeStatus, ARateMismatchReportsOneRefusalNamingBothRates)
{
    SushiDSP::InMemoryCabinetLog log;
    SushiDSP::CabinetNode node(log);
    node.prepare(prepare_info(256));

    node.set_cabinet(make_identity_ir(44100.0));

    ASSERT_EQ(log.refusals().size(), 1u);
    EXPECT_EQ(log.refusals()[0].cause, SushiDSP::CabinetRefusalCause::SampleRateMismatch);
    EXPECT_EQ(log.refusals()[0].impulse_response_rate, 44100.0);
    EXPECT_EQ(log.refusals()[0].node_rate, K_SAMPLE_RATE);
}

TEST(Small_CabinetNodeStatus, AnUnpreparedNodeIsRefusedWithItsOwnStatus)
{
    SushiDSP::CabinetNode node;

    EXPECT_EQ(node.set_cabinet(make_identity_ir(K_SAMPLE_RATE)), SushiDSP::CabinetStatus::NotPrepared);
}

TEST(Small_CabinetNodeStatus, AnUnpreparedNodeReportsOneRefusalForItsCause)
{
    SushiDSP::InMemoryCabinetLog log;
    SushiDSP::CabinetNode node(log);

    node.set_cabinet(make_identity_ir(K_SAMPLE_RATE));

    ASSERT_EQ(log.refusals().size(), 1u);
    EXPECT_EQ(log.refusals()[0].cause, SushiDSP::CabinetRefusalCause::NotPrepared);
}

TEST(Small_CabinetNodeStatus, AnInstalledCabinetReportsNoRefusal)
{
    SushiDSP::InMemoryCabinetLog log;
    SushiDSP::CabinetNode node(log);
    node.prepare(prepare_info(256));

    node.set_cabinet(make_identity_ir(K_SAMPLE_RATE));

    EXPECT_TRUE(log.refusals().empty());
}
