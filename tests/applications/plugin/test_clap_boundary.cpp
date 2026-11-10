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
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
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

namespace
{
    /// @brief The failure value the guarded calls return when they throw.
    constexpr int K_FALLBACK = -1;

    /// @brief Runs a call that throws a std::exception under the guard.
    int guarded_std_throw()
    {
        return SushiDSP::Plugin::guarded(
            "state.load", K_FALLBACK,
            []() -> int
            {
                throw std::runtime_error("bad preset");
            });
    }

    /// @brief Runs a call that throws something that is not a std::exception under the guard.
    bool guarded_non_std_throw()
    {
        return SushiDSP::Plugin::guarded(
            "params.flush", false,
            []() -> bool
            {
                throw 42;
            });
    }
} // namespace

TEST(Small_ClapBoundary, ACallThatReturnsKeepsItsOwnResult)
{
    SushiDSP::LogCapture capture;

    EXPECT_EQ(SushiDSP::Plugin::guarded("test.fine", K_FALLBACK, [] { return 7; }), 7);
}

TEST(Small_ClapBoundary, ACallThatReturnsWritesNothing)
{
    SushiDSP::LogCapture capture;

    static_cast<void>(SushiDSP::Plugin::guarded("test.fine", K_FALLBACK, [] { return 7; }));

    EXPECT_TRUE(capture.lines().empty());
}

TEST(Small_ClapBoundary, AStdExceptionReturnsTheFallback)
{
    SushiDSP::LogCapture capture;

    EXPECT_EQ(guarded_std_throw(), K_FALLBACK);
}

TEST(Small_ClapBoundary, AStdExceptionWritesOneErrorNamingTheCallback)
{
    SushiDSP::LogCapture capture;

    static_cast<void>(guarded_std_throw());

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "plugin");
    EXPECT_NE(lines[0].message.find("state.load"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("bad preset"), std::string::npos) << lines[0].message;
}

TEST(Small_ClapBoundary, ANonStdThrowReturnsTheFallback)
{
    SushiDSP::LogCapture capture;

    EXPECT_FALSE(guarded_non_std_throw());
}

TEST(Small_ClapBoundary, ANonStdThrowWritesOneErrorNamingTheCallback)
{
    SushiDSP::LogCapture capture;

    static_cast<void>(guarded_non_std_throw());

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("params.flush"), std::string::npos) << lines[0].message;
}
