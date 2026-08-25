/**************************************************************************/
/* test_buffer_pool.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

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
