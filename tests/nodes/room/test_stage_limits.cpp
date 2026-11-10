/**************************************************************************/
/* test_stage_limits.cpp                                                  */
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
 * @file test_stage_limits.cpp
 * @brief Tests the early-delay ceiling, which scales with the sample rate.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/stage_limits.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace
{
    /// @brief Holds one rate and the ceiling it gives.
    struct Ceiling
    {
            const char* name;
            double sample_rate;
            std::uint32_t frames;
    };

    /// @brief Holds the ceilings by hand from the definition (order + 1) * 12 m / 343 m/s * rate,
    /// rounded up: 5 * 12 / 343 is 0.174927 s, which is 8396.48 frames at 48 kHz and 16792.96 at
    /// 96 kHz. The first is the 8397 the header names.
    const std::array<Ceiling, 2> kCeilings = {{{"At48kHz", 48000.0, 8397u}, {"At96kHz", 96000.0, 16793u}}};

    /// @brief Names a ceiling case after its rate.
    std::string ceiling_name(const ::testing::TestParamInfo<Ceiling>& info)
    {
        return info.param.name;
    }
} // namespace

class Small_StageLimits : public ::testing::TestWithParam<Ceiling>
{
};

TEST_P(Small_StageLimits, EarlyDelayCeilingIsTheLongestCrossoverTimeInFrames)
{
    EXPECT_EQ(SushiDSP::Room::max_early_delay_frames(GetParam().sample_rate), GetParam().frames);
}

INSTANTIATE_TEST_SUITE_P(Rates, Small_StageLimits, ::testing::ValuesIn(kCeilings), ceiling_name);
