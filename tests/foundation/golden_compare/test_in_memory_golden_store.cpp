/**************************************************************************/
/* test_in_memory_golden_store.cpp                                        */
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
 * @file test_in_memory_golden_store.cpp
 * @brief Tests that the in-memory GoldenStore keeps what it is given and refuses what it lacks.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/golden_compare/in_memory_golden_store.hpp>

#include <vector>

namespace
{
    /// @brief Holds the samples the cases below store.
    const std::vector<float> K_SAMPLES{0.0f, 0.5f, -0.25f, 1.0f};
} // namespace

TEST(Small_InMemoryGoldenStore, SamplesWrittenToAPathAreReadBackExactly)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    ASSERT_TRUE(store.write("a/b.f32", K_SAMPLES));

    std::vector<float> read;
    ASSERT_TRUE(store.read("a/b.f32", read, K_SAMPLES.size()));

    EXPECT_EQ(read, K_SAMPLES);
}

TEST(Small_InMemoryGoldenStore, ReadingMoreThanTheEntryHoldsReturnsWhatItHolds)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    ASSERT_TRUE(store.write("a/b.f32", K_SAMPLES));

    std::vector<float> read;
    ASSERT_TRUE(store.read("a/b.f32", read, K_SAMPLES.size() + 3));

    EXPECT_EQ(read, K_SAMPLES);
}

TEST(Small_InMemoryGoldenStore, ReadingAPathNothingWasWrittenToFails)
{
    const SushiDSP::golden::InMemoryGoldenStore store;
    std::vector<float> read;

    EXPECT_FALSE(store.read("a/b.f32", read, 4));
}

TEST(Small_InMemoryGoldenStore, AWriteReplacesTheEntryAtThePath)
{
    SushiDSP::golden::InMemoryGoldenStore store;
    ASSERT_TRUE(store.write("a/b.f32", K_SAMPLES));
    ASSERT_TRUE(store.write("a/b.f32", std::vector<float>{2.0f}));

    std::vector<float> read;
    ASSERT_TRUE(store.read("a/b.f32", read, K_SAMPLES.size()));

    EXPECT_EQ(read, std::vector<float>{2.0f});
}
