/****************************************************************/
/* test_channel_selection.cpp                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/RigGraph.hpp"
#include "SushiDSP/audio_format/ChannelSelection.hpp"

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

TEST(Unit_Dsp, RigOutputAdaptsToDeviceCoversTheGraphsTable)
{
    EXPECT_TRUE(SushiDSP::rig_output_adapts_to_device(1u, 1u));
    EXPECT_TRUE(SushiDSP::rig_output_adapts_to_device(1u, 2u));
    EXPECT_TRUE(SushiDSP::rig_output_adapts_to_device(2u, 1u));
    EXPECT_TRUE(SushiDSP::rig_output_adapts_to_device(2u, 2u));

    EXPECT_EQ(SushiDSP::rig_output_adapts_to_device(1u, 1u), SushiDSP::RigGraph::channels_adapt(1u, 1u));
    EXPECT_EQ(SushiDSP::rig_output_adapts_to_device(1u, 2u), SushiDSP::RigGraph::channels_adapt(1u, 2u));
    EXPECT_EQ(SushiDSP::rig_output_adapts_to_device(2u, 1u), SushiDSP::RigGraph::channels_adapt(2u, 1u));
    EXPECT_EQ(SushiDSP::rig_output_adapts_to_device(2u, 2u), SushiDSP::RigGraph::channels_adapt(2u, 2u));
}

TEST(Unit_Dsp, RigOutputAdaptsToDeviceRefusesWidthsWithNoRule)
{
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(2u, 4u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(1u, 3u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(0u, 1u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(1u, 0u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(0u, 0u));
}

TEST(Unit_Dsp, RigOutputAdaptsToDeviceRefusesEqualWidthsTheWriterCannotWrite)
{
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(4u, 4u));
    EXPECT_FALSE(SushiDSP::rig_output_adapts_to_device(8u, 8u));
    EXPECT_TRUE(SushiDSP::RigGraph::channels_adapt(4u, 4u));
}

TEST(Unit_Dsp, WriteRigOutputCopiesOneRigChannelToBothDeviceChannels)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    const float* rig_outputs[1] = { mono.data() };
    std::vector<float> left(3, -1.0f), right(3, -1.0f);
    float* device_outputs[2] = { left.data(), right.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, mono);
    EXPECT_EQ(right, mono);
}

TEST(Unit_Dsp, WriteRigOutputZeroesTheChannelTheSelectionExcludesForOneRigChannel)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    const float* rig_outputs[1] = { mono.data() };
    std::vector<float> left(3, 9.0f), right(3, 9.0f);
    float* device_outputs[2] = { left.data(), right.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(left, mono);
    for (float sample : right) { EXPECT_FLOAT_EQ(sample, 0.0f); }

    left.assign(3, 9.0f);
    right.assign(3, 9.0f);
    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Right);
    for (float sample : left) { EXPECT_FLOAT_EQ(sample, 0.0f); }
    EXPECT_EQ(right, mono);
}

TEST(Unit_Dsp, WriteRigOutputPassesTwoRigChannelsStraightThrough)
{
    std::vector<float> rig_left{ 1.0f, 2.0f, 3.0f };
    std::vector<float> rig_right{ -1.0f, -2.0f, -3.0f };
    const float* rig_outputs[2] = { rig_left.data(), rig_right.data() };
    std::vector<float> left(3, 9.0f), right(3, 9.0f);
    float* device_outputs[2] = { left.data(), right.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_EQ(left, rig_left);
    EXPECT_EQ(right, rig_right);
}

TEST(Unit_Dsp, WriteRigOutputZeroesTheChannelTheSelectionExcludes)
{
    std::vector<float> rig_left{ 1.0f, 2.0f, 3.0f };
    std::vector<float> rig_right{ -1.0f, -2.0f, -3.0f };
    const float* rig_outputs[2] = { rig_left.data(), rig_right.data() };
    std::vector<float> left(3, 9.0f), right(3, 9.0f);
    float* device_outputs[2] = { left.data(), right.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Left);
    EXPECT_EQ(left, rig_left);
    for (float sample : right) { EXPECT_FLOAT_EQ(sample, 0.0f); }

    left.assign(3, 9.0f);
    right.assign(3, 9.0f);
    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 2u, 3u,
                                         SushiDSP::ChannelSelection::Right);
    for (float sample : left) { EXPECT_FLOAT_EQ(sample, 0.0f); }
    EXPECT_EQ(right, rig_right);
}

TEST(Unit_Dsp, WriteRigOutputSumsAndHalvesTwoRigChannelsIntoAMonoDevice)
{
    std::vector<float> rig_left{ 1.0f, 2.0f, 3.0f };
    std::vector<float> rig_right{ 3.0f, 0.0f, -1.0f };
    const float* rig_outputs[2] = { rig_left.data(), rig_right.data() };
    std::vector<float> device(3, 9.0f);
    float* device_outputs[1] = { device.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 2u, device_outputs, 1u, 3u,
                                         SushiDSP::ChannelSelection::Stereo);

    EXPECT_FLOAT_EQ(device[0], 2.0f);
    EXPECT_FLOAT_EQ(device[1], 1.0f);
    EXPECT_FLOAT_EQ(device[2], 1.0f);
}

TEST(Unit_Dsp, WriteRigOutputIgnoresTheSelectionOnAMonoDevice)
{
    std::vector<float> mono{ 1.0f, 2.0f, 3.0f };
    const float* rig_outputs[1] = { mono.data() };
    std::vector<float> device(3, 9.0f);
    float* device_outputs[1] = { device.data() };

    SushiDSP::write_rig_output_to_device(rig_outputs, 1u, device_outputs, 1u, 3u,
                                         SushiDSP::ChannelSelection::Right);

    EXPECT_EQ(device, mono);
}
