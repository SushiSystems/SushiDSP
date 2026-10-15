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

/** @file test_microphone_node.cpp
 *  @brief Tests the microphone rig element: polarity, the room flag and pass-through.
 *  @author Mustafa Garip
 */

#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/nodes/microphone/microphone_node.hpp>

using SushiDSP::MicrophoneNode;

TEST(Small_MicrophoneNode, MicrophonePassesItsInputThroughUnchanged)
{
    MicrophoneNode mic;
    const std::vector<float> input(4, 0.5f);
    std::vector<float> output(4, 0.0f);
    const float* inputs[1] = {input.data()};
    float* outputs[1] = {output.data()};
    SushiDSP::ProcessContext context{outputs, inputs, 4, 0.0};

    mic.process(context);

    for (float sample : output)
    {
        EXPECT_FLOAT_EQ(sample, 0.5f);
    }
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

TEST(Small_MicrophoneNode, MicrophoneDefaultsToACardioidWithProximity)
{
    MicrophoneNode mic;

    EXPECT_EQ(mic.pattern(), MicrophoneNode::Pattern::Cardioid);
    EXPECT_TRUE(mic.proximity());
    EXPECT_FLOAT_EQ(mic.get_parameter("pattern"), 1.0f);
    EXPECT_FLOAT_EQ(mic.get_parameter("proximity"), 1.0f);
}

TEST(Small_MicrophoneNode, MicrophonePatternReadsBackAndIsClampedToTheFourPatterns)
{
    MicrophoneNode mic;

    mic.set_parameter_for_control_thread("pattern", 3.0f);
    EXPECT_EQ(mic.pattern(), MicrophoneNode::Pattern::FigureEight);
    EXPECT_FLOAT_EQ(mic.get_parameter("pattern"), 3.0f);

    mic.set_parameter_for_control_thread("pattern", 9.0f);
    EXPECT_EQ(mic.pattern(), MicrophoneNode::Pattern::FigureEight);

    mic.set_parameter_for_control_thread("pattern", -2.0f);
    EXPECT_EQ(mic.pattern(), MicrophoneNode::Pattern::Omni);
}

TEST(Small_MicrophoneNode, MicrophoneProximitySwitchReadsBack)
{
    MicrophoneNode mic;

    mic.set_parameter_for_control_thread("proximity", 0.0f);

    EXPECT_FALSE(mic.proximity());
    EXPECT_FLOAT_EQ(mic.get_parameter("proximity"), 0.0f);
}

TEST(Small_MicrophoneNode, MicrophoneDeclaresOneChannelInAndOut)
{
    MicrophoneNode mic;

    EXPECT_EQ(mic.ports().input_channels, 1u);
    EXPECT_EQ(mic.ports().output_channels, 1u);
    EXPECT_STREQ(mic.type_id(), "sushidsp.microphone");
}
