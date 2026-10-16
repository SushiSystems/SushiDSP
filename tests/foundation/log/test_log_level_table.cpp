/**************************************************************************/
/* test_log_level_table.cpp                                               */
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
 * @file test_log_level_table.cpp
 * @brief Tests the per-category level table.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_level_table.hpp>

namespace
{
    /// @brief Holds the hash of a category named room.
    constexpr std::uint64_t K_ROOM = SushiDSP::log_category_hash("room");

    /// @brief Holds the hash of a category named cabinet.
    constexpr std::uint64_t K_CABINET = SushiDSP::log_category_hash("cabinet");
} // namespace

TEST(Small_LogLevelTable, CategoryWithNoLevelOfItsOwnReadsTheSharedLevel)
{
    SushiDSP::LogLevelTable table(SushiDSP::LogLevel::Warn);
    EXPECT_EQ(table.level_of(K_ROOM), SushiDSP::LogLevel::Warn);
    EXPECT_FALSE(table.has_own_level(K_ROOM));
}

TEST(Small_LogLevelTable, SetGivesOneCategoryItsOwnLevel)
{
    SushiDSP::LogLevelTable table(SushiDSP::LogLevel::Warn);
    EXPECT_TRUE(table.set(K_ROOM, SushiDSP::LogLevel::Trace));

    EXPECT_EQ(table.level_of(K_ROOM), SushiDSP::LogLevel::Trace);
    EXPECT_EQ(table.level_of(K_CABINET), SushiDSP::LogLevel::Warn);
    EXPECT_TRUE(table.has_own_level(K_ROOM));
}

TEST(Small_LogLevelTable, SetEveryOverwritesTheOwnLevelsAndSetSharedLeavesThem)
{
    SushiDSP::LogLevelTable table(SushiDSP::LogLevel::Warn);
    table.set(K_ROOM, SushiDSP::LogLevel::Trace);

    table.set_shared(SushiDSP::LogLevel::Info);
    EXPECT_EQ(table.level_of(K_ROOM), SushiDSP::LogLevel::Trace);
    EXPECT_EQ(table.level_of(K_CABINET), SushiDSP::LogLevel::Info);

    table.set_every(SushiDSP::LogLevel::Error);
    EXPECT_EQ(table.level_of(K_ROOM), SushiDSP::LogLevel::Error);
    EXPECT_EQ(table.level_of(K_CABINET), SushiDSP::LogLevel::Error);
}

TEST(Small_LogLevelTable, FullTableRefusesANewCategoryAndKeepsTheOnesItHolds)
{
    SushiDSP::LogLevelTable table(SushiDSP::LogLevel::Warn);
    for (std::uint64_t hash = 1; hash <= SushiDSP::K_MAX_LOG_CATEGORIES; ++hash)
        ASSERT_TRUE(table.set(hash, SushiDSP::LogLevel::Debug));

    EXPECT_FALSE(table.set(SushiDSP::K_MAX_LOG_CATEGORIES + 1, SushiDSP::LogLevel::Trace));
    EXPECT_EQ(table.level_of(SushiDSP::K_MAX_LOG_CATEGORIES + 1), SushiDSP::LogLevel::Warn);
    EXPECT_EQ(table.level_of(1), SushiDSP::LogLevel::Debug);
    EXPECT_TRUE(table.set(1, SushiDSP::LogLevel::Info));
}
