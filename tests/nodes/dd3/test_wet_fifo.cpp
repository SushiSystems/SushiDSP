/**************************************************************************/
/* test_wet_fifo.cpp                                                      */
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
 * @file test_wet_fifo.cpp
 * @brief Tests the wet FIFO's priming, its capacity and its starvation.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/dd3/wet_fifo.hpp>

namespace
{
    using SushiDSP::dd3::WetFifo;

    /// @brief Pushes `count` samples valued first, first + 1, first + 2 and so on. Integers are exact in a float.
    void push_ramp(WetFifo& fifo, std::uint32_t count, float first)
    {
        for (std::uint32_t n = 0; n < count; ++n)
        {
            fifo.push(first + static_cast<float>(n));
        }
    }

    /// @brief Pops `count` samples and discards them.
    void pop_many(WetFifo& fifo, std::uint32_t count)
    {
        for (std::uint32_t n = 0; n < count; ++n)
        {
            fifo.pop();
        }
    }

    /// @brief A reset FIFO.
    WetFifo fresh_fifo()
    {
        WetFifo fifo;
        fifo.reset();
        return fifo;
    }
} // namespace

TEST(Small_WetFifo, StartsPrimingAndPopsSilence)
{
    WetFifo fifo = fresh_fifo();

    EXPECT_TRUE(fifo.priming());
    EXPECT_EQ(fifo.pop(), 0.0f);
}

TEST(Small_WetFifo, PopsNothingUntilThePrimeIsMetOnRealSamples)
{
    WetFifo fifo = fresh_fifo();
    push_ramp(fifo, WetFifo::K_PRIME_FRAMES - 1u, 1.0f);

    EXPECT_TRUE(fifo.priming());
    EXPECT_EQ(fifo.pop(), 0.0f);
    EXPECT_EQ(fifo.depth(), WetFifo::K_PRIME_FRAMES - 1u);
}

TEST(Small_WetFifo, PopsTheOldestSampleOnceTheFifoIsPrimed)
{
    WetFifo fifo = fresh_fifo();
    push_ramp(fifo, WetFifo::K_PRIME_FRAMES, 1.0f);

    EXPECT_EQ(fifo.pop(), 1.0f);
    EXPECT_EQ(fifo.depth(), WetFifo::K_PRIME_FRAMES - 1u);
    EXPECT_FALSE(fifo.priming());
    EXPECT_EQ(fifo.pop(), 2.0f);
}

TEST(Small_WetFifo, DepthNeverExceedsItsCapacity)
{
    WetFifo fifo = fresh_fifo();

    push_ramp(fifo, 2u * WetFifo::K_CAPACITY, 1.0f);

    EXPECT_EQ(fifo.depth(), WetFifo::K_CAPACITY);
}

TEST(Small_WetFifo, DropsTheOldestSampleRatherThanTheNewestWhenFull)
{
    WetFifo fifo = fresh_fifo();
    const std::uint32_t overflow = 3u;

    push_ramp(fifo, WetFifo::K_CAPACITY + overflow, 1.0f);

    EXPECT_EQ(fifo.pop(), 1.0f + static_cast<float>(overflow));
    EXPECT_EQ(fifo.pop(), 2.0f + static_cast<float>(overflow));
}

TEST(Small_WetFifo, StarvedFifoPopsSilence)
{
    WetFifo fifo = fresh_fifo();
    push_ramp(fifo, WetFifo::K_PRIME_FRAMES, 1.0f);
    pop_many(fifo, WetFifo::K_PRIME_FRAMES);

    EXPECT_EQ(fifo.depth(), 0u);
    EXPECT_EQ(fifo.pop(), 0.0f);
}
