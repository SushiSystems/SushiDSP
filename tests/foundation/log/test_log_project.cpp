/**************************************************************************/
/* test_log_project.cpp                                                   */
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
 * @file test_log_project.cpp
 * @brief Tests the project a log category belongs to.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <string_view>

#include <SushiDSP/foundation/log/log_project.hpp>

TEST(Small_LogProject, HoldsTheNameAndLevelVariableItWasMadeWith)
{
    constexpr SushiDSP::LogProject project{"sushiruntime", "SR_LOG_LEVEL"};
    static_assert(project.name() == std::string_view("sushiruntime"), "the name is constexpr");
    EXPECT_EQ(project.level_variable(), std::string_view("SR_LOG_LEVEL"));
}