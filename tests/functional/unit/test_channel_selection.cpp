/**************************************************************************/
/* test_channel_selection.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/device/ChannelSelection.hpp"

TEST(Unit_Dsp, ParseChannelSelectionAcceptsLRAndStereo)
{
    EXPECT_EQ(SushiDSP::parse_channel_selection("L", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("l", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("R", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("r", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("stereo", "--output"), SushiDSP::ChannelSelection::Stereo);
}

TEST(Unit_Dsp, ParseChannelSelectionRejectsGarbage)
{
    EXPECT_THROW(SushiDSP::parse_channel_selection("banana", "--input"), std::runtime_error);
}

TEST(Unit_Dsp, ChannelSelectionInputIndexMatchesLeftRight)
{
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Left), 0u);
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Right), 1u);
}

TEST(Unit_Dsp, ChannelSelectionLabelsAreHumanReadable)
{
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Left), "L");
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Right), "R");
    EXPECT_STREQ(SushiDSP::channel_selection_label(SushiDSP::ChannelSelection::Stereo), "stereo");
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesBothChannelsForStereo)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, mono);
    EXPECT_EQ(right, mono);
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesOnlyLeftForLeft)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Left);

    EXPECT_EQ(left, mono);
    for (float sample : right) { EXPECT_FLOAT_EQ(sample, 0.0f); }
}

TEST(Unit_Dsp, FanOutMonoToStereoWritesOnlyRightForRight)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* outputs[2] = { left.data(), right.data() };

    SushiDSP::fan_out_mono_to_stereo(mono.data(), outputs, 3, SushiDSP::ChannelSelection::Right);

    for (float sample : left) { EXPECT_FLOAT_EQ(sample, 0.0f); }
    EXPECT_EQ(right, mono);
}
