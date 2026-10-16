/**************************************************************************/
/* test_golden_compare_log_category.cpp                                   */
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
 * @file test_golden_compare_log_category.cpp
 * @brief Tests the category the golden-file comparison logs under.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/golden_compare/golden_compare_log_category.hpp>

#include <string_view>

TEST(Small_GoldenCompareLogCategory, IsNamedGoldenCompare)
{
    EXPECT_EQ(K_GOLDEN_COMPARE_LOG_CATEGORY.module(), std::string_view("golden_compare"));
}
