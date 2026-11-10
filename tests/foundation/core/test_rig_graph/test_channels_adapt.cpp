/**************************************************************************/
/* test_channels_adapt.cpp                                                */
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
 * @file test_channels_adapt.cpp
 * @brief Tests which channel widths RigGraph adapts to one another.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/rig_graph.hpp>

namespace
{

/** A source width and a destination width, with whether RigGraph adapts the pair. */
struct AdaptRow
{
    const char* name;
    std::uint32_t from_channels;
    std::uint32_t to_channels;
    bool adapts;
};

// The rule (rig_graph.cpp): equal non-zero widths pass, and mono and stereo convert either way.
const AdaptRow kAdaptRows[] = {
    {"MonoToMono", 1u, 1u, true},
    {"MonoToStereo", 1u, 2u, true},
    {"StereoToMono", 2u, 1u, true},
    {"StereoToStereo", 2u, 2u, true},
    {"EqualWideWidths", 4u, 4u, true},
    {"StereoToQuad", 2u, 4u, false},
    {"ZeroToOne", 0u, 1u, false},
};

} // namespace

class Small_RigGraphChannelsAdapt : public ::testing::TestWithParam<AdaptRow>
{
};

TEST_P(Small_RigGraphChannelsAdapt, AnswersAsTheRuleSays)
{
    EXPECT_EQ(SushiDSP::RigGraph::channels_adapt(GetParam().from_channels, GetParam().to_channels), GetParam().adapts);
}

INSTANTIATE_TEST_SUITE_P(Widths, Small_RigGraphChannelsAdapt, ::testing::ValuesIn(kAdaptRows),
                         [](const ::testing::TestParamInfo<AdaptRow>& info) { return std::string(info.param.name); });
