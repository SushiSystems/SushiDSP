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
 * @file test_benchmark_harness.cpp
 * @brief Tests that measure() runs its repetitions on a timeline and costs the result.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/in_memory_nanosecond_clock.hpp>

#include "support/tolerance.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

constexpr std::uint32_t kRepetitions = SushiDSP::bench::K_WARMUP_PERIODS + SushiDSP::bench::K_ACTIVE_PERIODS;

constexpr double kCallNanoseconds = 1000.0;
constexpr std::uint32_t kBlockFrames = 256;
constexpr double kReferenceRate = 48000.0;

/** Skips a test when the build carries no profiler, since a run then records nothing. */
class Medium_BenchmarkHarnessRecorded : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        if (!SushiDSP::profile::K_PROFILER_BUILT)
        {
            GTEST_SKIP() << "this build records nothing";
        }
    }
};

/** Gives each test its own folder for a checks file, and removes it afterwards. */
class Medium_BenchmarkHarnessChecks : public Medium_BenchmarkHarnessRecorded
{
  protected:
    void SetUp() override
    {
        Medium_BenchmarkHarnessRecorded::SetUp();
        folder_ = std::filesystem::temp_directory_path() /
                  (std::string("sushidsp_harness_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::create_directories(folder_);
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(folder_, ignored);
    }

    /** Writes a checks file with the given text and returns its path. */
    std::string write_checks(const std::string& text) const
    {
        const std::filesystem::path path = folder_ / "bench.checks.json";
        std::ofstream file(path, std::ios::binary);
        file << text;
        return path.string();
    }

  private:
    std::filesystem::path folder_;
};

} // namespace

TEST(Medium_BenchmarkHarness, BenchmarkHarnessRunsTheBodyForEveryWarmupAndActiveRepetition)
{
    int calls = 0;

    SushiDSP::bench::measure("count", 1, kReferenceRate, [&] { ++calls; });

    EXPECT_EQ(calls, static_cast<int>(kRepetitions));
}

TEST_F(Medium_BenchmarkHarnessRecorded, BenchmarkHarnessRecordsEveryActivePeriod)
{
    const SushiDSP::bench::Result result = SushiDSP::bench::measure("budget", kBlockFrames, kReferenceRate, [] {});

    EXPECT_EQ(result.periods, SushiDSP::bench::K_ACTIVE_PERIODS);
    EXPECT_EQ(result.record.active_periods(), SushiDSP::bench::K_ACTIVE_PERIODS);
    EXPECT_EQ(result.record.warmup_periods(), SushiDSP::bench::K_WARMUP_PERIODS);
}

// The budget is 1e9 * 256 / 48000 = 5333333.33 ns: two rounded operations in double.
TEST_F(Medium_BenchmarkHarnessRecorded, BenchmarkHarnessBudgetsOneCallAtTheSamplesItMakes)
{
    const double expected_nanoseconds = 1e9 * kBlockFrames / kReferenceRate;

    const SushiDSP::bench::Result result = SushiDSP::bench::measure("budget", kBlockFrames, kReferenceRate, [] {});

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(result.budget_nanoseconds, expected_nanoseconds, 2)));
}

// The profile keeps the budget as whole nanoseconds, so it truncates the 5333333.33 by less than one.
TEST_F(Medium_BenchmarkHarnessRecorded, BenchmarkHarnessTruncatesTheTimelineBudgetToWholeNanoseconds)
{
    constexpr double kTruncationBound = 1.0;
    const double expected_nanoseconds = 1e9 * kBlockFrames / kReferenceRate;

    const SushiDSP::bench::Result result = SushiDSP::bench::measure("budget", kBlockFrames, kReferenceRate, [] {});
    const SushiDSP::profile::TimelineSummary summary =
        result.record.summarise(SushiDSP::profile::names::K_BENCH_TIMELINE);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(static_cast<double>(summary.budget_ns),
                                                                   expected_nanoseconds, kTruncationBound, 1.0)));
}

// 1000 ns * 256 frames / 1e6 = 0.256 ms: two rounded operations.
TEST(Small_BenchmarkHarness, ResultConvertsItsPerSampleCostToMillisecondsPerBlock)
{
    const SushiDSP::bench::Result result{"fixed", kCallNanoseconds, 0.0};

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(result.milliseconds_per_block(kBlockFrames), 0.256, 2)));
}

// One sample costs 1000 ns against a period of 1e9 / 48000 = 20833.33 ns, which is 4.8 percent.
TEST(Small_BenchmarkHarness, ResultFromCallsCostsTheMedianAgainstOneSamplePeriod)
{
    const SushiDSP::bench::Result result =
        SushiDSP::bench::result_from_calls("fixed", 1, kReferenceRate, std::vector<double>{kCallNanoseconds});

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<double>(result.realtime_percent, 4.8, 4)));
}

TEST(Small_BenchmarkHarness, TheUnrecordedPathRunsTheSameRepetitionsAsTheRecordedOne)
{
    int calls = 0;
    SushiDSP::bench::StepNanosecondClock clock{kCallNanoseconds};

    SushiDSP::bench::measure_unrecorded("stepped", 1, kReferenceRate, [&] { ++calls; }, clock);

    EXPECT_EQ(calls, static_cast<int>(kRepetitions));
}

TEST(Small_BenchmarkHarness, TheUnrecordedPathReportsOnePeriodPerActiveRepetition)
{
    SushiDSP::bench::StepNanosecondClock clock{kCallNanoseconds};

    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure_unrecorded("stepped", 1, kReferenceRate, [] {}, clock);

    EXPECT_EQ(result.periods, SushiDSP::bench::K_ACTIVE_PERIODS);
}

// Every repetition spans one clock step, so the median is the step, and the jitter between equal repetitions is 0.
TEST(Small_BenchmarkHarness, TheUnrecordedPathCostsEachRepetitionAtTheClockStep)
{
    SushiDSP::bench::StepNanosecondClock clock{kCallNanoseconds};

    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure_unrecorded("stepped", 1, kReferenceRate, [] {}, clock);

    EXPECT_EQ(result.nanoseconds_per_sample, kCallNanoseconds);
    EXPECT_EQ(result.jitter_nanoseconds, 0.0);
}

TEST(Small_BenchmarkHarness, TheUnrecordedPathRecordsNothing)
{
    SushiDSP::bench::StepNanosecondClock clock{kCallNanoseconds};

    const SushiDSP::bench::Result result =
        SushiDSP::bench::measure_unrecorded("stepped", 1, kReferenceRate, [] {}, clock);

    EXPECT_TRUE(result.record.empty());
}

namespace
{

// Sorted calls over 2 samples: rank 2 gives 3 (1.5 each), rank 3.96 gives 4 + 0.96 * 96 = 96.16 (48.08), the maximum is 100 (50).
const std::vector<double> kCallNanosecondsSample{1.0, 2.0, 3.0, 4.0, 100.0};
constexpr std::uint32_t kSamplesPerCall = 2;

SushiDSP::bench::Result sample_result()
{
    return SushiDSP::bench::result_from_calls("fixed", kSamplesPerCall, kReferenceRate, kCallNanosecondsSample);
}

} // namespace

TEST(Small_BenchmarkHarness, ResultFromCallsTakesTheMedianOfTheCalls)
{
    EXPECT_EQ(sample_result().nanoseconds_per_sample, 1.5);
}

// The rank 3.96 carries one rounding, the interpolation across 96 ns another, and the sum a third.
TEST(Small_BenchmarkHarness, ResultFromCallsTakesTheNinetyNinthPercentileOfTheCalls)
{
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<double>(sample_result().p99_nanoseconds_per_sample, 48.08, 3)));
}

TEST(Small_BenchmarkHarness, ResultFromCallsTakesTheMaximumOfTheCalls)
{
    EXPECT_EQ(sample_result().maximum_nanoseconds_per_sample, 50.0);
}

TEST(Small_BenchmarkHarness, ResultFromCallsCountsEveryCall)
{
    EXPECT_EQ(sample_result().periods, kCallNanosecondsSample.size());
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

TEST_F(Medium_BenchmarkHarnessChecks, ABoundOfOneNanosecondFailsAndNamesItsReasonAndItsKnownIssue)
{
    const std::string checks =
        write_checks(R"([{"subject": "timeline:bench.block", "statistic": "max", "at_most_ns": 1}])");
    const SushiDSP::bench::Result result = SushiDSP::bench::measure("tight", 1, kReferenceRate, [] {});

    testing::internal::CaptureStdout();
    const std::size_t failed = SushiDSP::bench::print_verdicts(result, checks, "KI-TEST");
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(failed, 1u);
    EXPECT_NE(printed.find("above at most 1 ns"), std::string::npos) << printed;
    EXPECT_NE(printed.find("[known issue KI-TEST]"), std::string::npos) << printed;
}

TEST_F(Medium_BenchmarkHarnessChecks, ALooseBoundPasses)
{
    const std::string checks = write_checks(R"([{"subject": "timeline:bench.block", "statistic": "median",
                     "at_most_ns": 60000000000}])");
    const SushiDSP::bench::Result result = SushiDSP::bench::measure("loose", 1, kReferenceRate, [] {});

    testing::internal::CaptureStdout();
    const std::size_t failed = SushiDSP::bench::print_verdicts(result, checks);
    const std::string printed = testing::internal::GetCapturedStdout();

    EXPECT_EQ(failed, 0u);
    EXPECT_NE(printed.find("check pass"), std::string::npos) << printed;
}
