/****************************************************************/
/* test_spsc_ring.cpp                                           */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_spsc_ring.cpp
 * @brief Tests the push, pop and ordering of SpscRing.
 * @author Mustafa Garip
 */

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/SpscRing.hpp"

TEST(Unit_Dsp, SpscRingPushThenPopReturnsSameValue)
{
    SushiDSP::SpscRing<int> ring(4);

    ASSERT_TRUE(ring.push(42));

    int value = 0;
    ASSERT_TRUE(ring.pop(value));
    EXPECT_EQ(value, 42);
}

TEST(Unit_Dsp, SpscRingPopOnEmptyReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(4);

    int value = 0;
    EXPECT_FALSE(ring.pop(value));
}

TEST(Unit_Dsp, SpscRingPushBeyondCapacityReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(2);

    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));
    EXPECT_FALSE(ring.push(3));
}

TEST(Unit_Dsp, SpscRingPreservesFifoOrder)
{
    SushiDSP::SpscRing<int> ring(4);

    ring.push(1);
    ring.push(2);
    ring.push(3);

    int a = 0, b = 0, c = 0;
    ring.pop(a);
    ring.pop(b);
    ring.pop(c);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST(Unit_Dsp, SpscRingConcurrentProducerConsumerDeliversAllValuesInOrder)
{
    constexpr int total = 100000;
    SushiDSP::SpscRing<int> ring(64);

    std::vector<int> consumed;
    consumed.reserve(total);

    std::thread producer([&ring, total]()
    {
        for (int i = 0; i < total; ++i)
        {
            while (!ring.push(i))
            {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&ring, &consumed, total]()
    {
        int value = 0;
        while (static_cast<int>(consumed.size()) < total)
        {
            if (ring.pop(value))
            {
                consumed.push_back(value);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(static_cast<int>(consumed.size()), total);
    for (int i = 0; i < total; ++i)
    {
        ASSERT_EQ(consumed[static_cast<std::size_t>(i)], i) << "mismatch at index " << i;
    }
}
