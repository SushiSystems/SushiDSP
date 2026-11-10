/**************************************************************************/
/* test_sine_tone_node.cpp                                                */
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
 * @file test_sine_tone_node.cpp
 * @brief Tests the range, frequency and reset of SineToneNode.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "sine_tone_node.hpp"

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// Runs one block of the node through a one-channel context and returns the samples.
    std::vector<float> render_block(SushiDSP::SineToneNode& node, std::uint32_t frames)
    {
        std::vector<float> output_buffer(frames, 0.0f);
        float* output_channels[1] = {output_buffer.data()};
        SushiDSP::ProcessContext context{output_channels, nullptr, frames, 0.0};
        node.process(context);
        return output_buffer;
    }

    /// Counts the places where the sign of consecutive samples changes, zero counting as not negative.
    std::ptrdiff_t count_sign_changes(const std::vector<float>& samples)
    {
        return std::inner_product(samples.begin() + 1, samples.end(), samples.begin(), std::ptrdiff_t{0},
                                  std::plus<std::ptrdiff_t>(),
                                  [](float next, float previous) { return (previous < 0.0f) != (next < 0.0f) ? 1 : 0; });
    }
} // namespace

TEST(Small_SineToneNode, SineToneNodeOutputStaysWithinUnitRange)
{
    SushiDSP::SineToneNode node(440.0f);
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, 256, 1});

    const std::vector<float> samples = render_block(node, 256);

    // The double sine never exceeds one, and narrowing to float keeps one as the largest value.
    EXPECT_TRUE(std::all_of(samples.begin(), samples.end(), [](float sample) { return std::abs(sample) <= 1.0f; }));
}

TEST(Small_SineToneNode, SineToneNodeProducesExpectedFrequency)
{
    constexpr float frequency = 1000.0f;
    constexpr std::uint32_t frames = 480;
    SushiDSP::SineToneNode node(frequency);
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, frames, 1});

    const std::vector<float> samples = render_block(node, frames);

    // Ten whole periods cross zero every 24 frames, at frames 24 to 456, which is 19 crossings.
    constexpr std::ptrdiff_t expected_crossings = 19;
    EXPECT_EQ(count_sign_changes(samples), expected_crossings);
}

TEST(Small_SineToneNode, SineToneNodeResetRestartsPhaseFromZero)
{
    SushiDSP::SineToneNode node(440.0f);
    node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, 8, 1});

    const std::vector<float> first = render_block(node, 8);
    node.reset();
    const std::vector<float> second = render_block(node, 8);

    // The phase restarts at the same double, so the same operations give the same bits.
    EXPECT_EQ(first, second);
}
