/**************************************************************************/
/* test_channel_selection.cpp                                             */
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
 * @file test_channel_selection.cpp
 * @brief Tests the parsing and mapping of ChannelSelection.
 * @author Mustafa Garip
 */

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/audio_format/channel_selection.hpp>

namespace
{

/** A rig width and a device width the output stage accepts. */
struct WidthPair
{
    const char* name;
    std::uint32_t rig_channels;
    std::uint32_t device_channels;
};

const WidthPair kAdaptingWidths[] = {
    {"MonoToMono", 1u, 1u},
    {"MonoToStereo", 1u, 2u},
    {"StereoToMono", 2u, 1u},
    {"StereoToStereo", 2u, 2u},
};

/** How many of kRigChannels the rig carries. */
struct RigWidth
{
    const char* name;
    std::uint32_t channels;
};

const RigWidth kRigWidths[] = {
    {"OneRigChannel", 1u},
    {"TwoRigChannels", 2u},
};

const std::array<std::vector<float>, 2> kRigChannels = {std::vector<float>{1.0f, 2.0f, 3.0f},
                                                         std::vector<float>{-1.0f, -2.0f, -3.0f}};

const std::vector<float> kSilence(3, 0.0f);

} // namespace

TEST(Small_ChannelSelection, ParseChannelSelectionAcceptsLRAndStereo)
{
    EXPECT_EQ(SushiDSP::parse_channel_selection("L", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("l", "--input"), SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(SushiDSP::parse_channel_selection("R", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("r", "--input"), SushiDSP::ChannelSelection::Right);
    EXPECT_EQ(SushiDSP::parse_channel_selection("stereo", "--output"),
              SushiDSP::ChannelSelection::Stereo);
}

TEST(Small_ChannelSelection, ParseChannelSelectionRejectsGarbage)
{
    EXPECT_THROW(SushiDSP::parse_channel_selection("banana", "--input"), std::runtime_error);
}

TEST(Small_ChannelSelection, ChannelSelectionInputIndexMatchesLeftRight)
{
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Left), 0u);
    EXPECT_EQ(SushiDSP::channel_selection_input_index(SushiDSP::ChannelSelection::Right), 1u);
}

class Small_RigOutputAdapts : public ::testing::TestWithParam<WidthPair>
{
};

TEST_P(Small_RigOutputAdapts, AcceptsTheWidthPairsTheGraphAdapts)
{
    EXPECT_TRUE(SushiDSP::rig_output_adapts_to_device(GetParam().rig_channels, GetParam().device_channels));
}

INSTANTIATE_TEST_SUITE_P(Widths, Small_RigOutputAdapts, ::testing::ValuesIn(kAdaptingWidths),
                         [](const ::testing::TestParamInfo<WidthPair>& info) { return std::string(info.param.name); });

TEST(Small_ChannelSelection, RigOutputAdaptsToDeviceRefusesWidthsWithNoRule)
{
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(2u, 4u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(1u, 3u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(0u, 1u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(1u, 0u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(0u, 0u));
}

TEST(Small_ChannelSelection, RigOutputAdaptsToDeviceRefusesEqualWidthsTheWriterCannotWrite)
{
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(4u, 4u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(8u, 8u));
}

TEST(Small_ChannelSelection, WriteRigOutputCopiesOneRigChannelToBothDeviceChannels)
{
    std::vector<float> mono{1.0f, 2.0f, 3.0f};
    const float* rig_outputs[1] = {mono.data()};
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* device_outputs[2] = {left.data(), right.data()};

    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, mono);
    EXPECT_EQ(right, mono);
}

class Small_WriteRigOutputExcludes : public ::testing::TestWithParam<RigWidth>
{
  protected:
    /** Writes the rig to a stereo device under a selection and returns the left and right channels. */
    std::array<std::vector<float>, 2> write(SushiDSP::ChannelSelection selection) const
    {
        const float* rig_outputs[2] = {kRigChannels[0].data(), kRigChannels[1].data()};
        std::array<std::vector<float>, 2> device = {std::vector<float>(3, 9.0f), std::vector<float>(3, 9.0f)};
        float* device_outputs[2] = {device[0].data(), device[1].data()};

        SushiDSP::write_rig_output_to_device(rig_outputs, GetParam().channels, device_outputs, 2u, 3u, selection);
        return device;
    }
};

// The last rig channel is the right device channel in both rows.
TEST_P(Small_WriteRigOutputExcludes, ZeroesTheRightChannelWhenLeftIsSelected)
{
    const std::array<std::vector<float>, 2> device = write(SushiDSP::ChannelSelection::Left);

    EXPECT_EQ(device[0], kRigChannels[0]);
    EXPECT_EQ(device[1], kSilence);
}

TEST_P(Small_WriteRigOutputExcludes, ZeroesTheLeftChannelWhenRightIsSelected)
{
    const std::array<std::vector<float>, 2> device = write(SushiDSP::ChannelSelection::Right);

    EXPECT_EQ(device[0], kSilence);
    EXPECT_EQ(device[1], kRigChannels[GetParam().channels - 1u]);
}

INSTANTIATE_TEST_SUITE_P(RigWidths, Small_WriteRigOutputExcludes, ::testing::ValuesIn(kRigWidths),
                         [](const ::testing::TestParamInfo<RigWidth>& info) { return std::string(info.param.name); });

TEST(Small_ChannelSelection, WriteRigOutputPassesTwoRigChannelsStraightThrough)
{
    std::vector<float> rig_left{1.0f, 2.0f, 3.0f};
    std::vector<float> rig_right{-1.0f, -2.0f, -3.0f};
    const float* rig_outputs[2] = {rig_left.data(), rig_right.data()};
    std::vector<float> left(3, 9.0f), right(3, 9.0f);
    float* device_outputs[2] = {left.data(), right.data()};

    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, rig_left);
    EXPECT_EQ(right, rig_right);
}

TEST(Small_ChannelSelection, WriteRigOutputSumsAndHalvesTwoRigChannelsIntoAMonoDevice)
{
    std::vector<float> rig_left{1.0f, 2.0f, 3.0f};
    std::vector<float> rig_right{3.0f, 0.0f, -1.0f};
    const float* rig_outputs[2] = {rig_left.data(), rig_right.data()};
    std::vector<float> device(3, 9.0f);
    float* device_outputs[1] = {device.data()};

    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 1u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_FLOAT_EQ(device[0], 2.0f);
    EXPECT_FLOAT_EQ(device[1], 1.0f);
    EXPECT_FLOAT_EQ(device[2], 1.0f);
}

TEST(Small_ChannelSelection, WriteRigOutputIgnoresTheSelectionOnAMonoDevice)
{
    std::vector<float> mono{1.0f, 2.0f, 3.0f};
    const float* rig_outputs[1] = {mono.data()};
    std::vector<float> device(3, 9.0f);
    float* device_outputs[1] = {device.data()};

    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 1u, 3u,
                                         SushiDSP::ChannelSelection::Right);

    EXPECT_EQ(device, mono);
}
