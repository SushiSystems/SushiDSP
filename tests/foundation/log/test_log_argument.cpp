/**************************************************************************/
/* test_log_argument.cpp                                                  */
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
 * @file test_log_argument.cpp
 * @brief Tests how a log argument writes its value into a bounded buffer.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_argument.hpp>

#include <cstddef>
#include <string>

namespace
{
    std::string render(const SushiDSP::LogArgument& argument, std::size_t capacity)
    {
        std::string buffer(capacity, '\0');
        const std::size_t written = argument.write_to(buffer.data(), capacity);
        return buffer.substr(0, written);
    }
} // namespace

TEST(LogArgument, IntegerWritesItsDecimalDigits)
{
    EXPECT_EQ(render(SushiDSP::LogArgument(42), 16), "42");
    EXPECT_EQ(render(SushiDSP::LogArgument(-7), 16), "-7");
}

TEST(LogArgument, BooleanWritesTrueOrFalse)
{
    EXPECT_EQ(render(SushiDSP::LogArgument(true), 16), "true");
    EXPECT_EQ(render(SushiDSP::LogArgument(false), 16), "false");
}

TEST(LogArgument, TextWritesItsCharacters)
{
    EXPECT_EQ(render(SushiDSP::LogArgument("track"), 16), "track");
    EXPECT_EQ(render(SushiDSP::LogArgument(std::string("take")), 16), "take");
}

TEST(LogArgument, WriteStopsAtTheBufferCapacity)
{
    EXPECT_EQ(render(SushiDSP::LogArgument("abcdef"), 3), "abc");
}
