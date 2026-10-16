/**************************************************************************/
/* test_scoped_log_sink.cpp                                               */
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
 * @file test_scoped_log_sink.cpp
 * @brief Tests that a scoped sink is installed for its scope and restored after it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/capturing_log_sink.hpp>
#include <SushiDSP/foundation/log/logger.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>
#include <SushiDSP/foundation/log/scoped_log_sink.hpp>
#include <SushiDSP/foundation/log/sushidsp_log_project.hpp>

#include <memory>

namespace
{
    const SushiDSP::LogCategory K_CATEGORY =
        SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "scope_test");
} // namespace

TEST(Small_ScopedLogSink, LinesGoToTheScopedSinkWhileItLives)
{
    const auto outer = std::make_shared<SushiDSP::CapturingLogSink>();
    const auto inner = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(outer, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);
    {
        SushiDSP::ScopedLogSink guard(logger, inner);
        logger.log(SushiDSP::LogLevel::Info, K_CATEGORY, SushiDSP::LogSite{__FILE__, __LINE__}, "inside");
    }
    EXPECT_EQ(inner->lines().size(), 1u);
    EXPECT_TRUE(outer->lines().empty());
}

TEST(Small_ScopedLogSink, PreviousSinkReceivesLinesAgainAfterTheScope)
{
    const auto outer = std::make_shared<SushiDSP::CapturingLogSink>();
    const auto inner = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(outer, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);
    {
        SushiDSP::ScopedLogSink guard(logger, inner);
    }
    logger.log(SushiDSP::LogLevel::Info, K_CATEGORY, SushiDSP::LogSite{__FILE__, __LINE__}, "after");
    EXPECT_EQ(outer->lines().size(), 1u);
}

TEST(Small_ScopedLogSink, FlushesTheScopedSinkWhenItEnds)
{
    const auto outer = std::make_shared<SushiDSP::CapturingLogSink>();
    const auto inner = std::make_shared<SushiDSP::CapturingLogSink>();
    SushiDSP::Logger logger(outer, SushiDSP::LogLevel::Trace, SushiDSP::LogDelivery::Synchronous);
    {
        SushiDSP::ScopedLogSink guard(logger, inner);
        EXPECT_EQ(inner->flush_count(), 0u);
    }
    EXPECT_GE(inner->flush_count(), 1u);
}
