/**************************************************************************/
/* test_sushidsp_log_project.cpp                                          */
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
 * @file test_sushidsp_log_project.cpp
 * @brief Tests the SushiLog project every SushiDSP category belongs to.
 * @author Mustafa Garip
 */

#include <string_view>

#include <gtest/gtest.h>

#include <SushiLog/log/log_category.hpp>

#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

TEST(Small_SushidspLogProject, IsSushidspSetBySdLogLevel)
{
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_PROJECT.name(), std::string_view("sushidsp"));
    EXPECT_EQ(SushiDSP::K_SUSHIDSP_LOG_PROJECT.level_variable(), std::string_view("SD_LOG_LEVEL"));
}

TEST(Small_SushidspLogProject, CategoryCarriesTheProjectAndItsModule)
{
    constexpr SushiLog::LogCategory category =
        SushiLog::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "room");
    EXPECT_EQ(category.project().name(), std::string_view("sushidsp"));
    EXPECT_EQ(category.module(), std::string_view("room"));
}