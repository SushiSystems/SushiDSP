/**************************************************************************/
/* test_microphone_node.cpp                                               */
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

/** @file test_microphone_node.cpp
 *  @brief Tests the microphone rig element: polarity, the room flag and pass-through.
 *  @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/microphone/microphone_node.hpp>

using SushiDSP::MicrophoneNode;

namespace
{
    /// The block every processing test pushes through the node.
    constexpr std::size_t K_FRAMES = 4;

    /// Runs one block of a constant through a microphone.
    /// @param mic The node.
    /// @param level The constant on the input.
    /// @return The output block.
    std::vector<float> process_constant(MicrophoneNode& mic, float level)
    {
        const std::vector<float> input(K_FRAMES, level);
        std::vector<float> output(K_FRAMES, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(K_FRAMES), 0.0};

        mic.process(context);
        return output;
    }

    /// One value written to the pattern parameter and the pattern it must select.
    struct PatternRow
    {
            const char* name;
            float written;
            MicrophoneNode::Pattern expected;
    };

    // The parameter rounds to the nearest choice and clamps to the four patterns.
    const PatternRow kPatternRows[] = {
        {"InRange", 3.0f, MicrophoneNode::Pattern::FigureEight},
        {"AboveTheLastPattern", 9.0f, MicrophoneNode::Pattern::FigureEight},
        {"BelowTheFirstPattern", -2.0f, MicrophoneNode::Pattern::Omni},
    };

    std::string pattern_name(const ::testing::TestParamInfo<PatternRow>& info)
    {
        return info.param.name;
    }

    class Small_MicrophoneNodePattern : public ::testing::TestWithParam<PatternRow>
    {
    };
} // namespace

TEST(Small_MicrophoneNode, MicrophonePassesItsInputThroughUnchanged)
{
    MicrophoneNode mic;

    EXPECT_EQ(process_constant(mic, 0.5f), std::vector<float>(K_FRAMES, 0.5f));
}

TEST(Small_MicrophoneNode, MicrophonePolarityInvertsItsOutput)
{
    MicrophoneNode mic;
    mic.set_parameter_for_control_thread("polarity", 1.0f);
    const std::vector<float> input(4, 0.5f);
    std::vector<float> output(4, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 4, 0.0};

    mic.process(context);

    EXPECT_FLOAT_EQ(output[0], -0.5f);
}

TEST(Small_MicrophoneNode, MicrophoneRoomFlagReadsBackAndMarksAnOverhead)
{
    MicrophoneNode mic;
    EXPECT_FALSE(mic.is_room_microphone());

    mic.set_parameter_for_control_thread("room", 1.0f);

    EXPECT_TRUE(mic.is_room_microphone());
    EXPECT_FLOAT_EQ(mic.get_parameter("room"), 1.0f);
}

TEST(Small_MicrophoneNode, MicrophoneDefaultsToACardioid)
{
    MicrophoneNode mic;

    EXPECT_EQ(mic.pattern(), MicrophoneNode::Pattern::Cardioid);
    EXPECT_FLOAT_EQ(mic.get_parameter("pattern"), 1.0f);
}

TEST(Small_MicrophoneNode, MicrophoneDefaultsToProximityOn)
{
    MicrophoneNode mic;

    EXPECT_TRUE(mic.proximity());
    EXPECT_FLOAT_EQ(mic.get_parameter("proximity"), 1.0f);
}

TEST_P(Small_MicrophoneNodePattern, MicrophonePatternReadsBackAndIsClampedToTheFourPatterns)
{
    MicrophoneNode mic;

    mic.set_parameter_for_control_thread("pattern", GetParam().written);

    EXPECT_EQ(mic.pattern(), GetParam().expected);
    EXPECT_FLOAT_EQ(mic.get_parameter("pattern"), static_cast<float>(GetParam().expected));
}

INSTANTIATE_TEST_SUITE_P(Writes, Small_MicrophoneNodePattern, ::testing::ValuesIn(kPatternRows), pattern_name);

TEST(Small_MicrophoneNode, MicrophoneProximitySwitchReadsBack)
{
    MicrophoneNode mic;

    mic.set_parameter_for_control_thread("proximity", 0.0f);

    EXPECT_FALSE(mic.proximity());
    EXPECT_FLOAT_EQ(mic.get_parameter("proximity"), 0.0f);
}
