/**************************************************************************/
/* test_null_test.cpp                                                     */
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
 * @file test_null_test.cpp
 * @brief Tests that the null test aligns a delayed copy and reports a mismatch when it is left unaligned.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/null_test.hpp>

#include <cstddef>
#include <vector>

namespace
{
    /// @brief A deterministic pseudo-random sequence in [-1, 1], so the test has no seed to vary.
    std::vector<float> pseudo_random(std::size_t length)
    {
        std::vector<float> values(length);
        std::uint32_t state = 12345u;
        for (std::size_t i = 0; i < length; ++i)
        {
            state = state * 1664525u + 1013904223u;
            values[i] = static_cast<float>(state) / 2147483648.0f - 1.0f;
        }
        return values;
    }
} // namespace

TEST(NullTest, AlignedDelayedCopyNullsCompletely)
{
    const std::vector<float> a = pseudo_random(4096);
    std::vector<float> b(a.size(), 0.0f);
    for (std::size_t i = 3; i < b.size(); ++i)
    {
        b[i] = a[i - 3];
    }
    EXPECT_LT(SushiDSP::measure::null_test(a, b, 5), -60.0);
}

TEST(NullTest, UnalignedDelayedCopyLeavesAResidual)
{
    const std::vector<float> a = pseudo_random(4096);
    std::vector<float> b(a.size(), 0.0f);
    for (std::size_t i = 3; i < b.size(); ++i)
    {
        b[i] = a[i - 3];
    }
    EXPECT_GT(SushiDSP::measure::null_test(a, b, 0), -20.0);
}
