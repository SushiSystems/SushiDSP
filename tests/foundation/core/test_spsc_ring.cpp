/**************************************************************************/
/* test_spsc_ring.cpp                                                     */
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
 * @file test_spsc_ring.cpp
 * @brief Tests the push, pop and ordering of SpscRing.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <functional>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/spsc_ring.hpp>

namespace
{
    /// @brief Holds how many values a first-in first-out row pushes and pops.
    struct CountRow
    {
            const char* name;
            int count;
    };

    const CountRow kCounts[] = {{"OneValue", 1}, {"ThreeValues", 3}, {"UpToCapacity", 4}};

    /// @brief Returns 0, 1, ... count - 1.
    std::vector<int> sequence(int count)
    {
        std::vector<int> values(static_cast<std::size_t>(count));
        std::iota(values.begin(), values.end(), 0);
        return values;
    }

    /// @brief Pushes every value of @p values into @p ring, yielding while it is full.
    void push_all(SushiDSP::SpscRing<int>& ring, const std::vector<int>& values)
    {
        for (const int value : values)
        {
            while (!ring.push(value))
            {
                std::this_thread::yield();
            }
        }
    }

    /// @brief Pops @p count values from @p ring in order, yielding while it is empty.
    std::vector<int> pop_all(SushiDSP::SpscRing<int>& ring, int count)
    {
        std::vector<int> popped;
        int value = 0;
        while (static_cast<int>(popped.size()) < count)
        {
            if (ring.pop(value))
            {
                popped.push_back(value);
            }
            else
            {
                std::this_thread::yield();
            }
        }
        return popped;
    }
} // namespace

TEST(Small_SpscRing, SpscRingPopOnEmptyReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(4);

    int value = 0;
    EXPECT_FALSE(ring.pop(value));
}

TEST(Small_SpscRing, SpscRingPushBeyondCapacityReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(2);

    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));
    EXPECT_FALSE(ring.push(3));
}

class Small_SpscRingOrder : public ::testing::TestWithParam<CountRow>
{
};

TEST_P(Small_SpscRingOrder, SpscRingPreservesFifoOrder)
{
    SushiDSP::SpscRing<int> ring(4);
    const std::vector<int> pushed = sequence(GetParam().count);

    push_all(ring, pushed);

    EXPECT_EQ(pop_all(ring, GetParam().count), pushed);
}

INSTANTIATE_TEST_SUITE_P(Fill, Small_SpscRingOrder, ::testing::ValuesIn(kCounts),
                         [](const ::testing::TestParamInfo<CountRow>& info) { return std::string(info.param.name); });

TEST(Medium_SpscRing, SpscRingConcurrentProducerConsumerDeliversAllValuesInOrder)
{
    constexpr int K_TOTAL = 100000;
    SushiDSP::SpscRing<int> ring(64);
    const std::vector<int> expected = sequence(K_TOTAL);
    std::vector<int> consumed;

    std::thread producer(push_all, std::ref(ring), std::cref(expected));
    std::thread consumer([&ring, &consumed]() { consumed = pop_all(ring, K_TOTAL); });
    producer.join();
    consumer.join();

    EXPECT_EQ(consumed, expected);
}
