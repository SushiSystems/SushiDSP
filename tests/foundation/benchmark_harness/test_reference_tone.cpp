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
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>

namespace
{

constexpr std::uint32_t kOneSecondOfFrames = 48000;

/** Counts the samples where the tone passes from below zero to zero or above. */
int count_rising_crossings(const std::vector<float>& tone)
{
    int rising = 0;
    for (std::size_t i = 1; i < tone.size(); ++i)
    {
        if (tone[i - 1] < 0.0f && tone[i] >= 0.0f)
        {
            ++rising;
        }
    }
    return rising;
}

/** The most a sampled sine can fall short of its peak: half a phase step, plus the float rounding of the product. */
double peak_shortfall_bound()
{
    const double pi = std::acos(-1.0);
    const double half_step = pi * SushiDSP::bench::K_REFERENCE_TONE_HZ / SushiDSP::bench::K_SAMPLE_RATE;
    const double level = SushiDSP::bench::K_REFERENCE_TONE_LEVEL;
    return level * (1.0 - std::cos(half_step)) + 2.0 * TestSupport::unit_roundoff<float>() * level;
}

} // namespace

TEST(Small_ReferenceTone, ReferenceToneHasTheRequestedLength)
{
    EXPECT_EQ(SushiDSP::bench::reference_tone(256).size(), 256u);
}

TEST(Small_ReferenceTone, ReferenceToneStartsAtZero)
{
    EXPECT_EQ(SushiDSP::bench::reference_tone(256)[0], 0.0f);
}

// The tone peaks where a sample lands within half a phase step of the crest.
TEST(Small_ReferenceTone, ReferenceTonePeaksAtAQuarterOfFullScale)
{
    const std::vector<float> tone = SushiDSP::bench::reference_tone(kOneSecondOfFrames);
    const auto [low, high] = std::minmax_element(tone.begin(), tone.end());
    const double level = SushiDSP::bench::K_REFERENCE_TONE_LEVEL;

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(*high, level, peak_shortfall_bound(), 1.0)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(*low, -level, peak_shortfall_bound(), 1.0)));
}

// The first cycle starts at zero and is not a crossing, so one second of 220 Hz holds 219 rising crossings.
TEST(Small_ReferenceTone, ReferenceToneCompletesTwoHundredTwentyCyclesASecond)
{
    constexpr int kRisingCrossings = 219;

    EXPECT_EQ(count_rising_crossings(SushiDSP::bench::reference_tone(kOneSecondOfFrames)), kRisingCrossings);
}
