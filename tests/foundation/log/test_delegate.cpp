/**************************************************************************/
/* test_delegate.cpp                                                      */
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
 * @file test_delegate.cpp
 * @brief Tests the callable held in a fixed buffer.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <memory>

#include <SushiDSP/foundation/log/delegate.hpp>

namespace
{
    /// @brief Holds a delegate of the shape the log sink uses.
    using IntDelegate = SushiDSP::Delegate<int(int), 48>;
} // namespace

TEST(Small_Delegate, EmptyDelegateIsFalse)
{
    EXPECT_FALSE(static_cast<bool>(IntDelegate()));
    EXPECT_FALSE(static_cast<bool>(IntDelegate(nullptr)));
}

TEST(Small_Delegate, CallsTheCallableItHolds)
{
    const int offset = 5;
    const IntDelegate add(
        [offset](int value)
        {
            return value + offset;
        });
    ASSERT_TRUE(static_cast<bool>(add));
    EXPECT_EQ(add(2), 7);
}

TEST(Small_Delegate, CopyHoldsItsOwnCallableAndDestroysIt)
{
    const auto counter = std::make_shared<int>(0);
    {
        IntDelegate first(
            [counter](int value)
            {
                return value + *counter;
            });
        const IntDelegate second = first;
        IntDelegate third;
        third = second;
        EXPECT_EQ(counter.use_count(), 4);
        EXPECT_EQ(third(1), 1);
    }
    EXPECT_EQ(counter.use_count(), 1);
}