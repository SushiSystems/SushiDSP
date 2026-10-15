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

#include <string_view>

TEST(LogCategory, NameIsTheNameItWasMadeWith)
{
    constexpr SushiDSP::LogCategory category = SushiDSP::make_log_category("room");
    EXPECT_EQ(category.name(), std::string_view("room"));
}

TEST(LogCategory, ConstructorAndFactoryAgree)
{
    const SushiDSP::LogCategory direct("rig");
    const SushiDSP::LogCategory made = SushiDSP::make_log_category("rig");
    EXPECT_EQ(direct.name(), made.name());
}
