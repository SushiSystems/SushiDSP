/**************************************************************************/
/* test_log_site.cpp                                                      */
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
 * @file test_log_site.cpp
 * @brief Tests the source position a log call carries.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/log_site.hpp>

TEST(Small_LogSite, FileNameIsThePartAfterTheLastSlashOrBackslash)
{
    EXPECT_EQ(SushiDSP::log_site_file_name("modules/foundation/log/source/logger.cpp"), "logger.cpp");
    EXPECT_EQ(SushiDSP::log_site_file_name("D:\\Projects\\a\\b.cpp"), "b.cpp");
    EXPECT_EQ(SushiDSP::log_site_file_name("plain.cpp"), "plain.cpp");
}

TEST(Small_LogSite, HoldsTheFileAndLineItWasMadeWith)
{
    const SushiDSP::LogSite site{__FILE__, __LINE__};
    EXPECT_EQ(SushiDSP::log_site_file_name(site.file), "test_log_site.cpp");
    EXPECT_GT(site.line, 0);
}
