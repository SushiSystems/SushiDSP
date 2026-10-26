/**************************************************************************/
/* test_clap_boundary.cpp                                                 */
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
 * @file test_clap_boundary.cpp
 * @brief Tests that the guard turns a throw into a fallback and one ERROR naming the callback.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include "clap_boundary.hpp"

#include <stdexcept>
#include <string>
#include <vector>

TEST(Small_ClapBoundary, ACallThatReturnsKeepsItsOwnResultAndWritesNothing)
{
    SushiDSP::LogCapture capture;

    const int result = SushiDSP::Plugin::guarded("test.fine", -1, [] { return 7; });

    EXPECT_EQ(result, 7);
    EXPECT_TRUE(capture.lines().empty());
}

TEST(Small_ClapBoundary, AStdExceptionReturnsTheFallbackAndWritesOneErrorNamingTheCallback)
{
    SushiDSP::LogCapture capture;

    const int result = SushiDSP::Plugin::guarded(
        "state.load", -1,
        []() -> int
        {
            throw std::runtime_error("bad preset");
        });

    EXPECT_EQ(result, -1);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "plugin");
    EXPECT_NE(lines[0].message.find("state.load"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("bad preset"), std::string::npos) << lines[0].message;
}

TEST(Small_ClapBoundary, ANonStdThrowReturnsTheFallbackAndWritesOneError)
{
    SushiDSP::LogCapture capture;

    const bool result = SushiDSP::Plugin::guarded(
        "params.flush", false,
        []() -> bool
        {
            throw 42;
        });

    EXPECT_FALSE(result);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("params.flush"), std::string::npos) << lines[0].message;
}
