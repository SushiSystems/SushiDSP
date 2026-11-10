/**************************************************************************/
/* test_audio_taper_pot.cpp                                               */
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
 * @file test_audio_taper_pot.cpp
 * @brief The audio taper's endpoints, its law and its monotonicity.
 * @author Mustafa Garip & Sushi Systems
 */

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/audio_taper_pot.hpp>

TEST(Small_AudioTaperPot, AudioTaperPotSpansZeroToUnity)
{
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(1.0f), 1.0f);
}

TEST(Small_AudioTaperPot, AudioTaperPotIsPositionToTheThreeHalves)
{
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.5f), 0.5f * std::sqrt(0.5f));
    EXPECT_FLOAT_EQ(SushiDSP::audio_taper_pot(0.25f), 0.25f * std::sqrt(0.25f));
}

namespace
{
    /// @brief Holds two positions, the first below the second, for the monotonic rows.
    struct Pair
    {
            const char* name;
            float lower;
            float higher;
    };

    /// @brief Holds one position strictly inside the pot's travel.
    struct Position
    {
            const char* name;
            float value;
    };

    const Pair kPairs[] = {
        {"BottomToQuarter", 0.0f, 0.25f}, {"QuarterToHalf", 0.25f, 0.5f}, {"HalfToThreeQuarters", 0.5f, 0.75f},
        {"ThreeQuartersToTop", 0.75f, 1.0f}, {"NeighboursAtTheBottom", 0.01f, 0.02f}, {"NeighboursAtTheTop", 0.98f, 0.99f},
    };

    const Position kInteriorPositions[] = {
        {"OneHundredth", 0.01f}, {"Quarter", 0.25f}, {"Half", 0.5f}, {"ThreeQuarters", 0.75f}, {"NinetyNinePercent", 0.99f},
    };
} // namespace

class Small_AudioTaperPotPairs : public ::testing::TestWithParam<Pair>
{
};

TEST_P(Small_AudioTaperPotPairs, AudioTaperPotRisesWithPosition)
{
    EXPECT_LT(SushiDSP::audio_taper_pot(GetParam().lower), SushiDSP::audio_taper_pot(GetParam().higher));
}

INSTANTIATE_TEST_SUITE_P(Travel, Small_AudioTaperPotPairs, ::testing::ValuesIn(kPairs),
                         [](const ::testing::TestParamInfo<Pair>& info) { return std::string(info.param.name); });

class Small_AudioTaperPotInterior : public ::testing::TestWithParam<Position>
{
};

TEST_P(Small_AudioTaperPotInterior, AudioTaperPotSitsUnderALinearPot)
{
    EXPECT_LT(SushiDSP::audio_taper_pot(GetParam().value), GetParam().value);
}

INSTANTIATE_TEST_SUITE_P(Travel, Small_AudioTaperPotInterior, ::testing::ValuesIn(kInteriorPositions),
                         [](const ::testing::TestParamInfo<Position>& info) { return std::string(info.param.name); });
