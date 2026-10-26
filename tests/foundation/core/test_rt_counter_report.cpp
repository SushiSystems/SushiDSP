/**************************************************************************/
/* test_rt_counter_report.cpp                                             */
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
 * @file test_rt_counter_report.cpp
 * @brief Tests the reporter that turns moved counters into one line.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/rt_counter.hpp>
#include <SushiDSP/foundation/core/rt_counter_report.hpp>

#include <chrono>
#include <string>

namespace
{
    using Clock = SushiDSP::RtCounterReporter::Clock;

    /// @brief Holds the interval every reporter below speaks at.
    constexpr std::chrono::seconds K_INTERVAL{5};

    /// @brief Holds the time every reporter below starts at; the clock is never read.
    const Clock::time_point K_START{};
} // namespace

TEST(Small_RtCounterReporter, TheFirstPollOnlyStartsTheClock)
{
    SushiDSP::RtCounter counter;
    counter.increment();
    SushiDSP::RtCounterReporter reporter(K_INTERVAL);
    reporter.add("no_callback", counter);

    EXPECT_TRUE(reporter.poll(K_START).empty());
}

TEST(Small_RtCounterReporter, APollBeforeTheIntervalHasPassedReturnsNothing)
{
    SushiDSP::RtCounter counter;
    SushiDSP::RtCounterReporter reporter(K_INTERVAL);
    reporter.add("no_callback", counter);
    static_cast<void>(reporter.poll(K_START));
    counter.increment();

    EXPECT_TRUE(reporter.poll(K_START + std::chrono::seconds(4)).empty());
}

TEST(Small_RtCounterReporter, ALineNamesEachCounterThatMovedAndByHowMuch)
{
    SushiDSP::RtCounter no_callback;
    SushiDSP::RtCounter oversize;
    SushiDSP::RtCounter quiet;
    SushiDSP::RtCounterReporter reporter(K_INTERVAL);
    reporter.add("no_callback", no_callback);
    reporter.add("oversize_block", oversize);
    reporter.add("overrun", quiet);
    static_cast<void>(reporter.poll(K_START));
    no_callback.increment(3U);
    oversize.note(1024U);

    const std::string line = reporter.poll(K_START + std::chrono::seconds(5));

    EXPECT_EQ(line, "3 no_callback, 1 oversize_block (largest 1024) in 5 s");
}

TEST(Small_RtCounterReporter, NothingIsReportedWhenNoCounterMoved)
{
    SushiDSP::RtCounter counter;
    SushiDSP::RtCounterReporter reporter(K_INTERVAL);
    reporter.add("no_callback", counter);
    static_cast<void>(reporter.poll(K_START));

    EXPECT_TRUE(reporter.poll(K_START + std::chrono::seconds(5)).empty());
}

TEST(Small_RtCounterReporter, ALaterLineCountsOnlyTheEventsSinceTheLastOne)
{
    SushiDSP::RtCounter counter;
    SushiDSP::RtCounterReporter reporter(K_INTERVAL);
    reporter.add("overrun", counter);
    static_cast<void>(reporter.poll(K_START));
    counter.increment(2U);
    static_cast<void>(reporter.poll(K_START + std::chrono::seconds(5)));
    counter.increment(1U);

    EXPECT_EQ(reporter.poll(K_START + std::chrono::seconds(10)), "1 overrun in 5 s");
}

