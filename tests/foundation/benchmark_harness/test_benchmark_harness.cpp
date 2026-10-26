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
 * @brief Tests that measure() runs its repetitions on a timeline and costs the result.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

TEST(Medium_BenchmarkHarness, BenchmarkHarnessRunsTheBodyForEveryWarmupAndActiveRepetition)
{
    int calls = 0;
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure("count", 1, 48000.0, [&] { ++calls; });
    EXPECT_EQ(calls, 2020);
    EXPECT_STREQ(result.name, "count");
}

TEST(Medium_BenchmarkHarness, BenchmarkHarnessRecordsTwoThousandActivePeriodsWithTheBudgetOfOneCall)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";

    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure("budget", 256, 48000.0, [] {});

    EXPECT_EQ(result.periods, 2000u);
    EXPECT_EQ(result.record.active_periods(), 2000u);
    EXPECT_EQ(result.record.warmup_periods(), 20u);
    EXPECT_NEAR(result.budget_nanoseconds, 256.0 / 48000.0 * 1e9, 1e-6);
    const SushiDSP::profile::TimelineSummary summary =
        result.record.summarise(SushiDSP::profile::names::K_BENCH_TIMELINE);
    EXPECT_NEAR(summary.budget_ns, 5333333.0, 1.0);
}

TEST(Medium_BenchmarkHarness, BenchmarkHarnessCostsTheResultAgainstOneSamplePeriod)
{
    const SushiDSP::bench::Result result{"fixed", 2083.333333, 0.0};
    // 2083.33 ns per sample over a 256-frame block is 0.5333 ms.
    EXPECT_NEAR(result.milliseconds_per_block(256), 0.53333, 1e-5);

    const SushiDSP::bench::Result measured =
        SushiDSP::bench::measure("percent", 1, 48000.0, [] {});
    EXPECT_NEAR(measured.realtime_percent,
                100.0 * measured.nanoseconds_per_sample / (1e9 / 48000.0), 1e-9);
}

TEST(Small_BenchmarkHarness, TheUnrecordedPathRunsTheSameRepetitionsAndRecordsNothing)
{
    int calls = 0;
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure_unrecorded("steady", 1, 48000.0, [&] { ++calls; });

    EXPECT_EQ(calls, 2020);
    EXPECT_EQ(result.periods, 2000u);
    EXPECT_TRUE(result.record.empty());
}

TEST(Small_BenchmarkHarness, ResultFromCallsTakesTheMedianTheNinetyNinthPercentileAndTheMaximum)
{
    const std::vector<double> call_ns{1.0, 2.0, 3.0, 4.0, 100.0};

    const SushiDSP::bench::Result result =
        SushiDSP::bench::result_from_calls("fixed", 2, 48000.0, call_ns);

    EXPECT_DOUBLE_EQ(result.nanoseconds_per_sample, 1.5);
    EXPECT_NEAR(result.p99_nanoseconds_per_sample, 96.16 / 2.0, 1e-9);
    EXPECT_DOUBLE_EQ(result.maximum_nanoseconds_per_sample, 50.0);
    EXPECT_EQ(result.periods, 5u);
}

TEST(Small_BenchmarkHarness, AResultWithNoRecordIsNotJudgedAndFailsNothing)
{
    const SushiDSP::bench::Result result{"unrecorded", 1.0, 0.0};

    testing::internal::CaptureStdout();
    const std::size_t failed = SushiDSP::bench::print_verdicts(result, "any.checks.json");
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(failed, 0u);
    EXPECT_NE(printed.find("not judged"), std::string::npos) << printed;
}

TEST(Medium_BenchmarkHarness, ABoundOfOneNanosecondFailsAndNamesItsReasonAndItsKnownIssue)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    const std::filesystem::path checks =
        std::filesystem::temp_directory_path() / "sushidsp_harness_tight.checks.json";
    {
        std::ofstream file(checks, std::ios::binary);
        file << R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 1}])";
    }
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure("tight", 1, 48000.0, [] {});

    testing::internal::CaptureStdout();
    const std::size_t failed =
        SushiDSP::bench::print_verdicts(result, checks.string(), "KI-TEST");
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(failed, 1u);
    EXPECT_NE(printed.find("above at most 1 ns"), std::string::npos) << printed;
    EXPECT_NE(printed.find("[known issue KI-TEST]"), std::string::npos) << printed;
    std::filesystem::remove(checks);
}

TEST(Medium_BenchmarkHarness, ALooseBoundPasses)
{
    if (!SushiDSP::profile::K_PROFILER_BUILT)
        GTEST_SKIP() << "this build records nothing";
    const std::filesystem::path checks =
        std::filesystem::temp_directory_path() / "sushidsp_harness_loose.checks.json";
    {
        std::ofstream file(checks, std::ios::binary);
        file << R"([{"subject": "timeline:bench.block", "statistic": "median",
                     "at_most_ns": 60000000000}])";
    }
    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure("loose", 1, 48000.0, [] {});

    testing::internal::CaptureStdout();
    const std::size_t failed = SushiDSP::bench::print_verdicts(result, checks.string());
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(failed, 0u);
    EXPECT_NE(printed.find("check pass"), std::string::npos) << printed;
    std::filesystem::remove(checks);
}
