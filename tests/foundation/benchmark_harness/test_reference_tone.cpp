/**************************************************************************/
/* test_reference_tone.cpp                                                */
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
 * @file test_reference_tone.cpp
 * @brief Tests that reference_tone() returns the 220 Hz tone at a quarter of full scale.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>

TEST(Small_ReferenceTone, ReferenceToneHasTheRequestedLengthAndStartsAtZero)
{
    const std::vector<float> tone = SushiDSP::bench::reference_tone(256);
    ASSERT_EQ(tone.size(), 256u);
    EXPECT_FLOAT_EQ(tone[0], 0.0f);
}

TEST(Small_ReferenceTone, ReferenceTonePeaksAtAQuarterOfFullScale)
{
    // One 220 Hz period at 48 kHz is 218.18 samples, so a second's worth reaches its peak.
    const std::vector<float> tone = SushiDSP::bench::reference_tone(48000);
    const auto [low, high] = std::minmax_element(tone.begin(), tone.end());
    EXPECT_NEAR(*high, 0.25f, 1e-4f);
    EXPECT_NEAR(*low, -0.25f, 1e-4f);
}

TEST(Small_ReferenceTone, ReferenceToneCompletesTwoHundredTwentyCyclesASecond)
{
    const std::vector<float> tone = SushiDSP::bench::reference_tone(48000);
    int rising = 0;
    for (std::size_t i = 1; i < tone.size(); ++i)
    {
        if (tone[i - 1] < 0.0f && tone[i] >= 0.0f)
            ++rising;
    }
    EXPECT_EQ(rising, 219);
}
