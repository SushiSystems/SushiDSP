/**************************************************************************/
/* test_log_category.cpp                                                  */
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
 * @file test_log_category.cpp
 * @brief Tests that a log category keeps the name it was made with.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_category.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

#include <string_view>

TEST(Small_LogCategory, HoldsTheProjectAndModuleItWasMadeWith)
{
    constexpr SushiDSP::LogCategory category =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "room");
    EXPECT_EQ(category.module(), std::string_view("room"));
    EXPECT_EQ(category.project().name(), std::string_view("sushidsp"));
}

TEST(Small_LogCategory, ConstructorAndFactoryAgree)
{
    const SushiDSP::LogCategory direct(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "rig");
    const SushiDSP::LogCategory made =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "rig");
    EXPECT_EQ(direct.module(), made.module());
}

TEST(Small_LogCategory, HashIgnoresLetterCaseAndTellsTwoNamesApart)
{
    constexpr SushiDSP::LogCategory room =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "room");
    static_assert(room.hash() == SushiDSP::log_category_hash("ROOM"), "the hash ignores case");
    EXPECT_NE(room.hash(), SushiDSP::log_category_hash("rooms"));
    EXPECT_NE(SushiDSP::log_category_hash(""), 0u);
}

TEST(Small_LogCategory, HashOfTheEmptyNameIsTheFnvOffsetBasis)
{
    EXPECT_EQ(SushiDSP::log_category_hash(""), 14695981039346656037ull);
}
