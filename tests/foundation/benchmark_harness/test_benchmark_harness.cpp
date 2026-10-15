/**************************************************************************/
/* test_benchmark_harness.cpp                                             */
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
 * @file test_benchmark_harness.cpp
 * @brief Tests that measure() times its body and costs the result against the sample rate.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>

TEST(Small_BenchmarkHarness, BenchmarkHarnessRunsTheBodyForEveryWarmupAndTimedRepetition)
{
    int calls = 0;
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure("count", 1, 48000.0, [&] { ++calls; });
    EXPECT_EQ(calls, 220);
    EXPECT_STREQ(result.name, "count");
}

TEST(Small_BenchmarkHarness, BenchmarkHarnessCostsTheResultAgainstOneSamplePeriod)
{
    const SushiDSP::bench::Result result{"fixed", 2083.333333, 0.0};
    // 2083.33 ns per sample over a 256-frame block is 0.5333 ms.
    EXPECT_NEAR(result.milliseconds_per_block(256), 0.53333, 1e-5);

    const SushiDSP::bench::Result measured =
        SushiDSP::bench::measure("percent", 1, 48000.0, [] {});
    EXPECT_NEAR(measured.realtime_percent,
                100.0 * measured.nanoseconds_per_sample / (1e9 / 48000.0), 1e-9);
}
