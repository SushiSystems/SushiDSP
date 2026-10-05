/****************************************************************/
/* test_buffer_pool.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_buffer_pool.cpp
 * @brief Tests that BufferPool hands out, counts and reuses its buffers.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "SushiDSP/core/BufferPool.hpp"

TEST(Unit_Dsp, BufferPoolAcquireReturnsDistinctBuffersUpToCapacity)
{
    SushiDSP::BufferPool pool(256, 2);

    float* a = pool.acquire(256);
    float* b = pool.acquire(256);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

TEST(Unit_Dsp, BufferPoolCapacityReportsMaxLiveBuffers)
{
    SushiDSP::BufferPool pool(256, 3);

    EXPECT_EQ(pool.capacity(), 3u);

    ASSERT_NE(pool.acquire(256), nullptr);
    ASSERT_NE(pool.acquire(256), nullptr);
    ASSERT_NE(pool.acquire(256), nullptr);
    EXPECT_EQ(pool.acquire(256), nullptr);
    EXPECT_EQ(pool.capacity(), 3u);
}

TEST(Unit_Dsp, BufferPoolReusesReleasedBuffer)
{
    SushiDSP::BufferPool pool(256, 1);

    float* a = pool.acquire(256);
    pool.release(a);
    float* b = pool.acquire(256);

    EXPECT_EQ(a, b);
}
