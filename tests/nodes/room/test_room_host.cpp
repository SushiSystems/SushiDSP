/**************************************************************************/
/* test_room_host.cpp                                                     */
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
 * @file test_room_host.cpp
 * @brief Tests that the room host names the reason an HRTF set was refused, and "installed" when it was not.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/room_host.hpp>

#include <array>
#include <string>

namespace
{
    using SushiDSP::room::HrtfStatus;

    /// @brief Holds one status and the text the host gives for it.
    struct StatusText
    {
            const char* name;
            HrtfStatus status;
            const char* text;
    };

    const std::array<StatusText, 4> kStatusTexts = {{
        {"Installed", HrtfStatus::Installed, "installed"},
        {"SampleRateMismatch", HrtfStatus::SampleRateMismatch, "the set's sample rate differs from the node's"},
        {"DecoderRefused", HrtfStatus::DecoderRefused, "the binaural decoder could not be prepared from the set"},
        {"PublishRefused", HrtfStatus::PublishRefused, "the render set could not be rebuilt with the set"},
    }};

    /// @brief Names a status case after the status.
    std::string status_name(const ::testing::TestParamInfo<StatusText>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_RoomHostStatus : public ::testing::TestWithParam<StatusText>
{
};

TEST_P(Small_RoomHostStatus, HrtfStatusTextNamesTheReason)
{
    EXPECT_STREQ(SushiDSP::room::hrtf_status_text(GetParam().status), GetParam().text);
}

INSTANTIATE_TEST_SUITE_P(Statuses, Small_RoomHostStatus, ::testing::ValuesIn(kStatusTexts), status_name);
