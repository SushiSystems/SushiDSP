/**************************************************************************/
/* test_log_compile_time_cut.cpp                                          */
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
 * @file test_log_compile_time_cut.cpp
 * @brief Tests that a log call below SD_LOG_MIN_LEVEL compiles to nothing.
 * @author Mustafa Garip
 */

/// @brief Cuts TRACE, DEBUG and INFO out of this translation unit, whatever the build type.
#define SD_LOG_MIN_LEVEL SD_LOG_LEVEL_WARN

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/log/CapturingLogSink.hpp"
#include "SushiDSP/log/Log.hpp"
#include "SushiDSP/log/ScopedLogSink.hpp"

namespace
{
    /// @brief Names the lines this file logs.
    inline constexpr SushiDSP::LogCategory K_CUT_TEST_LOG_CATEGORY =
        SushiDSP::make_log_category("cut_test");
}

TEST(Unit_Log, CallBelowTheCompileTimeMinimumDoesNotEvaluateItsArguments)
{
    const std::shared_ptr<SushiDSP::CapturingLogSink> capture =
        std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger&         logger = SushiDSP::default_logger();
    const SushiDSP::LogLevel  level_before = logger.minimum_level();
    int                       evaluations = 0;
    {
        const SushiDSP::ScopedLogSink guard(logger, capture);
        // The runtime level admits everything, so only the compile-time cut can stop a line.
        logger.set_minimum_level(SushiDSP::LogLevel::Trace);

        SD_LOG_TRACE(K_CUT_TEST_LOG_CATEGORY, "trace {}", ++evaluations);
        SD_LOG_DEBUG(K_CUT_TEST_LOG_CATEGORY, "debug {}", ++evaluations);
        SD_LOG_INFO(K_CUT_TEST_LOG_CATEGORY, "info {}", ++evaluations);
        EXPECT_EQ(evaluations, 0);
        EXPECT_TRUE(capture->lines().empty());

        SD_LOG_WARN(K_CUT_TEST_LOG_CATEGORY, "warn {}", ++evaluations);
        EXPECT_EQ(evaluations, 1);
    }
    logger.set_minimum_level(level_before);

    const std::vector<SushiDSP::CapturedLogLine> lines = capture->lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].message, "warn 1");
}
