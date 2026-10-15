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
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file test_spsc_ring.cpp
 * @brief Tests the push, pop and ordering of SpscRing.
 * @author Mustafa Garip
 */

#include <atomic>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/spsc_ring.hpp>

TEST(Medium_SpscRing, SpscRingPushThenPopReturnsSameValue)
{
    SushiDSP::SpscRing<int> ring(4);

    ASSERT_TRUE(ring.push(42));

    int value = 0;
    ASSERT_TRUE(ring.pop(value));
    EXPECT_EQ(value, 42);
}

TEST(Medium_SpscRing, SpscRingPopOnEmptyReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(4);

    int value = 0;
    EXPECT_FALSE(ring.pop(value));
}

TEST(Medium_SpscRing, SpscRingPushBeyondCapacityReturnsFalse)
{
    SushiDSP::SpscRing<int> ring(2);

    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));
    EXPECT_FALSE(ring.push(3));
}

TEST(Medium_SpscRing, SpscRingPreservesFifoOrder)
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

TEST(Medium_SpscRing, SpscRingConcurrentProducerConsumerDeliversAllValuesInOrder)
{
    constexpr int total = 100000;
    SushiDSP::SpscRing<int> ring(64);

    std::vector<int> consumed;
    consumed.reserve(total);

    std::thread producer(
        [&ring, total]()
        {
            for (int i = 0; i < total; ++i)
            {
                while (!ring.push(i))
                {
                    std::this_thread::yield();
                }
            }
        });

    std::thread consumer(
        [&ring, &consumed, total]()
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
