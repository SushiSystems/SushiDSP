/**************************************************************************/
/* test_fir_filter.cpp                                                    */
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
 * @file test_fir_filter.cpp
 * @brief Tests that a FIR filter convolves its input with the loaded taps over the history it keeps.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/fir_filter.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    using SushiDSP::room::FirFilter;

    /// @brief Counts the frames of every block the tests process.
    constexpr std::uint32_t K_FRAMES = 8;

    /// @brief Holds the lag of the delayed tap, past one block so that it reads the earlier one.
    constexpr std::size_t K_LAG = 9;

    using Block = std::array<float, K_FRAMES>;

    /// @brief Returns taps that are zero but for a one at @p lag.
    std::vector<float> taps_with_one_at(std::size_t lag)
    {
        std::vector<float> taps(FirFilter::K_TAPS, 0.0f);
        taps[lag] = 1.0f;
        return taps;
    }

    /// @brief Returns a prepared filter loaded with @p taps and cleared.
    FirFilter loaded_filter(const std::vector<float>& taps)
    {
        FirFilter filter;
        filter.prepare(K_FRAMES);
        filter.load(taps.data());
        filter.reset();
        return filter;
    }

    /// @brief Returns the block a filter writes for @p input.
    Block process_block(FirFilter& filter, const Block& input)
    {
        Block output{};
        filter.process(input.data(), output.data(), K_FRAMES);
        return output;
    }

    /// @brief Returns the naive convolution of @p signal with @p taps, with zeros before the
    /// signal starts, over the length of the signal.
    std::vector<float> naive_convolution(const std::vector<float>& signal, const std::vector<float>& taps)
    {
        std::vector<float> result(signal.size(), 0.0f);
        for (std::size_t frame = 0; frame < signal.size(); ++frame)
        {
            for (std::size_t lag = 0; lag <= frame && lag < taps.size(); ++lag)
            {
                result[frame] += taps[lag] * signal[frame - lag];
            }
        }
        return result;
    }

    /// @brief Returns the part of @p signal that is block @p index.
    Block block_of(const std::vector<float>& signal, std::size_t index)
    {
        Block block{};
        for (std::size_t frame = 0; frame < block.size(); ++frame)
        {
            block[frame] = signal[index * K_FRAMES + frame];
        }
        return block;
    }

    /// @brief Returns two blocks end to end.
    std::vector<float> joined(const Block& first, const Block& second)
    {
        std::vector<float> signal(first.begin(), first.end());
        signal.insert(signal.end(), second.begin(), second.end());
        return signal;
    }

    /// @brief Holds the first block of the tests that carry history: small integers.
    const Block K_RAMP = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
} // namespace

TEST(Small_FirFilter, UnitImpulseTapPassesTheInputUnchanged)
{
    // A tap of one at lag zero and zeros elsewhere multiplies by one and adds zeros, which is exact.
    FirFilter filter = loaded_filter(taps_with_one_at(0u));
    const Block input = {1.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f};

    EXPECT_EQ(process_block(filter, input), input);
}

TEST(Small_FirFilter, FirFilterOutputIsTheConvolutionOverTheHistoryOfEarlierBlocks)
{
    // Small integers and power-of-two taps are exact in float, so the match is bit for bit; lag 9 reads the block before.
    std::vector<float> taps = taps_with_one_at(K_LAG);
    taps[0] = 0.5f;
    taps[1] = 0.25f;
    FirFilter filter = loaded_filter(taps);
    const Block silence{};

    const Block first = process_block(filter, K_RAMP);
    const Block second = process_block(filter, silence);

    const std::vector<float> expected = naive_convolution(joined(K_RAMP, silence), taps);
    EXPECT_EQ(first, block_of(expected, 0u));
    EXPECT_EQ(second, block_of(expected, 1u));
}

TEST(Small_FirFilter, FirFilterLoadCopiesTheCoefficients)
{
    std::vector<float> taps = taps_with_one_at(0u);
    FirFilter filter = loaded_filter(taps);

    taps.assign(taps.size(), 0.0f);

    EXPECT_EQ(process_block(filter, K_RAMP), K_RAMP);
}

TEST(Small_FirFilter, FirFilterKeepsTheHistoryWhenTheCoefficientsChange)
{
    FirFilter filter = loaded_filter(taps_with_one_at(0u));
    const Block silence{};
    process_block(filter, K_RAMP);

    const std::vector<float> delayed = taps_with_one_at(K_LAG);
    filter.load(delayed.data());

    const std::vector<float> expected = naive_convolution(joined(K_RAMP, silence), delayed);
    EXPECT_EQ(process_block(filter, silence), block_of(expected, 1u));
}

TEST(Small_FirFilter, FirFilterResetZeroesTheHistory)
{
    FirFilter filter = loaded_filter(taps_with_one_at(K_LAG));
    process_block(filter, K_RAMP);

    filter.reset();

    const Block silence{};
    EXPECT_EQ(process_block(filter, silence), silence);
}

TEST(Small_FirFilter, FirFilterCarryFromCopiesTheHistoryAndNotTheCoefficients)
{
    FirFilter source = loaded_filter(taps_with_one_at(0u));
    process_block(source, K_RAMP);
    const std::vector<float> delayed = taps_with_one_at(K_LAG);
    FirFilter target = loaded_filter(delayed);
    const Block silence{};

    target.carry_from(source);

    // The target keeps its own lag-9 tap over the source's history; copied taps would pass silence as silence.
    const std::vector<float> expected = naive_convolution(joined(K_RAMP, silence), delayed);
    EXPECT_EQ(process_block(target, silence), block_of(expected, 1u));
}
