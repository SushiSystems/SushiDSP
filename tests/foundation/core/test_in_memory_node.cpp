/**************************************************************************/
/* test_in_memory_node.cpp                                                */
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
 * @file test_in_memory_node.cpp
 * @brief Tests the mix, the parameters and the record of the in-memory AudioNode.
 * @author Mustafa Garip
 */

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/in_memory_node.hpp>

namespace
{
    /// @brief One shape, one input and the first sample each output channel of port 0 must hold.
    struct MixRow
    {
            /// @brief Names the row in the test output.
            const char* name;
            /// @brief Holds the layout of the node.
            SushiDSP::PortLayout ports;
            /// @brief Holds the gain.
            float gain;
            /// @brief Holds the level of every channel of port 0.
            float level;
            /// @brief Holds the value of each input channel.
            std::vector<float> inputs;
            /// @brief Holds the value each output channel must hold.
            std::vector<float> expected;
    };

    /// @brief Holds the channels of a one-frame block and the context that points at them.
    class OneFrameBlock
    {
        public:
            /// @brief Builds input channels holding @p inputs and @p outputs output channels holding -1.
            OneFrameBlock(const std::vector<float>& inputs, std::uint32_t outputs)
                : inputs_(inputs), input_pointers_(inputs.size()), outputs_(outputs, -1.0f), output_pointers_(outputs)
            {
                for (std::size_t channel = 0; channel < inputs_.size(); ++channel)
                {
                    input_pointers_[channel] = &inputs_[channel];
                }
                for (std::size_t channel = 0; channel < outputs_.size(); ++channel)
                {
                    output_pointers_[channel] = &outputs_[channel];
                }
            }

            /// @brief Runs @p node over the block and returns the first sample of each output channel.
            std::vector<float> run(SushiDSP::AudioNode& node)
            {
                SushiDSP::ProcessContext context{output_pointers_.data(),
                                                 input_pointers_.empty() ? nullptr : input_pointers_.data(), 1,
                                                 0.0};
                node.process(context);
                return outputs_;
            }

        private:
            std::vector<float> inputs_;
            std::vector<const float*> input_pointers_;
            std::vector<float> outputs_;
            std::vector<float*> output_pointers_;
    };

    /// @brief Returns the signal of @p gain with every channel of port 0 at @p level.
    SushiDSP::InMemoryNodeSignal signal_of(float gain, float level)
    {
        SushiDSP::InMemoryNodeSignal signal;
        signal.gain = gain;
        signal.levels[0].fill(level);
        return signal;
    }

    /// @brief Returns the shape of @p ports with every other field unset.
    SushiDSP::InMemoryNodeShape shape_of(SushiDSP::PortLayout ports)
    {
        SushiDSP::InMemoryNodeShape shape;
        shape.ports = ports;
        return shape;
    }

    const MixRow K_MIX_ROWS[] = {
        {"ASourceWritesItsLevel", {0, 1}, 1.0f, 0.5f, {}, {0.5f}},
        {"AMonoNodeScalesItsInputAndAddsItsLevel", {1, 1}, 3.0f, 1.0f, {2.0f}, {7.0f}},
        {"TwoInputsFoldIntoOneOutputBySum", {2, 1}, 1.0f, 0.0f, {3.0f, 4.0f}, {7.0f}},
        {"AStereoNodeKeepsItsChannelsApart", {2, 2}, 2.0f, 0.0f, {1.0f, -1.0f}, {2.0f, -2.0f}},
        {"AMonoInputRepeatsAcrossAStereoOutput", {1, 2}, 1.0f, 0.0f, {0.5f}, {0.5f, 0.5f}},
    };

    class Small_InMemoryNodeMix : public ::testing::TestWithParam<MixRow>
    {
    };
} // namespace

TEST_P(Small_InMemoryNodeMix, WritesGainTimesTheFoldedInputPlusTheLevel)
{
    const MixRow& row = GetParam();
    SushiDSP::InMemoryNode node(shape_of(row.ports), signal_of(row.gain, row.level));
    OneFrameBlock block(row.inputs, row.ports.output_channels);
    EXPECT_EQ(block.run(node), row.expected);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_InMemoryNodeMix, ::testing::ValuesIn(K_MIX_ROWS),
                         [](const ::testing::TestParamInfo<MixRow>& info) { return info.param.name; });

TEST(Small_InMemoryNode, ANullInputReadsAsSilence)
{
    SushiDSP::InMemoryNode node(shape_of({1, 1}), signal_of(2.0f, 0.25f));
    OneFrameBlock block({}, 1);
    EXPECT_EQ(block.run(node), std::vector<float>({0.25f}));
}

TEST(Small_InMemoryNode, WritesEachDeclaredPortThroughThePortTable)
{
    SushiDSP::InMemoryNodeShape shape = shape_of({0, 1});
    shape.output_port_count = 2;
    shape.port_channels = std::array<std::uint32_t, SushiDSP::K_IN_MEMORY_NODE_MAX_PORTS>{1, 2};
    SushiDSP::InMemoryNodeSignal signal;
    signal.levels[1] = {0.75f, 0.5f};
    SushiDSP::InMemoryNode node(shape, signal);

    float mono = 0.0f;
    float left = 0.0f;
    float right = 0.0f;
    float* port_zero[1] = {&mono};
    float* port_one[2] = {&left, &right};
    float* const* table[2] = {port_zero, port_one};
    SushiDSP::ProcessContext context{port_zero, nullptr, 1, 0.0, table};
    node.process(context);

    EXPECT_EQ(std::vector<float>({mono, left, right}), std::vector<float>({0.0f, 0.75f, 0.5f}));
}

TEST(Small_InMemoryNode, AnUnsetShapeAnswersWithTheAudioNodeDefaults)
{
    const SushiDSP::InMemoryNode node(shape_of({1, 2}), {});
    EXPECT_EQ(std::vector<std::uint32_t>({node.output_port_count(), node.output_port_channels(0),
                                          node.output_port_channels(1), node.latency_samples()}),
              std::vector<std::uint32_t>({1u, 2u, 0u, 0u}));
}

TEST(Small_InMemoryNode, ASetShapeAnswersWithWhatItDeclares)
{
    SushiDSP::InMemoryNodeShape shape;
    shape.output_port_count = 3;
    shape.port_channels = std::array<std::uint32_t, SushiDSP::K_IN_MEMORY_NODE_MAX_PORTS>{1, 2, 1};
    shape.latency_samples = 7;
    const SushiDSP::InMemoryNode node(shape, {});
    EXPECT_EQ(std::vector<std::uint32_t>({node.output_port_count(), node.output_port_channels(1),
                                          node.latency_samples()}),
              std::vector<std::uint32_t>({3u, 2u, 7u}));
}

TEST(Small_InMemoryNode, ASetTypeIdIsTheOneItAnswers)
{
    SushiDSP::InMemoryNodeShape shape;
    shape.type_id = "test.in_memory";
    const SushiDSP::InMemoryNode node(shape, {});
    EXPECT_STREQ(node.type_id(), "test.in_memory");
}

TEST(Small_InMemoryNode, TheLevelIndexSetsEveryChannelOfPortZero)
{
    SushiDSP::InMemoryNode node(shape_of({0, 2}), signal_of(1.0f, 0.0f));
    node.set_parameter_for_audio_thread(SushiDSP::K_IN_MEMORY_NODE_LEVEL_INDEX, 0.25f);
    OneFrameBlock block({}, 2);
    EXPECT_EQ(block.run(node), std::vector<float>({0.25f, 0.25f}));
}

TEST(Small_InMemoryNode, TheGainIdReadsBackWhatItWrote)
{
    SushiDSP::InMemoryNode node(shape_of({1, 1}), signal_of(1.0f, 0.0f));
    node.set_parameter_for_control_thread("gain", 4.0f);
    EXPECT_EQ(node.get_parameter("gain"), 4.0f);
}

TEST(Small_InMemoryNode, AnUnknownIndexChangesNothing)
{
    SushiDSP::InMemoryNode node(shape_of({0, 1}), signal_of(1.0f, 0.5f));
    node.set_parameter_for_audio_thread(9, 3.0f);
    OneFrameBlock block({}, 1);
    EXPECT_EQ(block.run(node), std::vector<float>({0.5f}));
}

TEST(Small_InMemoryNode, RecordsTheIndexOfTheLastAudioThreadWrite)
{
    SushiDSP::InMemoryNode node(shape_of({0, 1}), signal_of(1.0f, 0.5f));
    node.set_parameter_for_audio_thread(9, 3.0f);
    EXPECT_EQ(node.record().last_audio_parameter_index, 9);
}

TEST(Small_InMemoryNode, AnUnknownIdReadsZero)
{
    const SushiDSP::InMemoryNode node(shape_of({1, 1}), signal_of(2.0f, 0.0f));
    EXPECT_EQ(node.get_parameter("no_such_parameter"), 0.0f);
}

TEST(Small_InMemoryNode, RecordsTheFirstSampleOfAnInput)
{
    SushiDSP::InMemoryNode node(shape_of({1, 1}), signal_of(1.0f, 0.0f));
    OneFrameBlock block({0.5f}, 1);
    block.run(node);
    EXPECT_EQ(node.record().first_input_samples[0], 0.5f);
}

TEST(Small_InMemoryNode, RecordsNanForANullInput)
{
    SushiDSP::InMemoryNode node(shape_of({1, 1}), signal_of(1.0f, 0.0f));
    OneFrameBlock block({}, 1);
    block.run(node);
    EXPECT_TRUE(std::isnan(node.record().first_input_samples[0]));
}

TEST(Small_InMemoryNode, CountsItsDestructionInTheRecordItWasGiven)
{
    SushiDSP::InMemoryNodeRecord record;
    auto node = std::make_unique<SushiDSP::InMemoryNode>(shape_of({1, 1}), SushiDSP::InMemoryNodeSignal{}, record);
    node.reset();
    EXPECT_EQ(record.destroyed, 1u);
}
