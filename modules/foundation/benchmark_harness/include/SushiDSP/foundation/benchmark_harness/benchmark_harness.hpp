/**************************************************************************/
/* benchmark_harness.hpp                                                  */
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
 * @file benchmark_harness.hpp
 * @brief Defines the timing harness and the table row the benchmark cases print.
 * @author Mustafa Garip
 */

#pragma once

#include <SushiDSP/foundation/profile/profile_record.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#ifndef SUSHIDSP_TESTS_DIR
/// @brief The folder the checks files of the benchmarks are read from; the build sets it.
#define SUSHIDSP_TESTS_DIR "."
#endif

/// @brief Holds the timing harness the benchmark programs share.
namespace SushiDSP::bench
{
    /// @brief Holds the reference sample rate every case is costed against.
    inline constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Holds the block length a device callback hands the audio thread.
    inline constexpr std::uint32_t K_BLOCK_SIZE = 256;

    /// @brief Holds the periods a case runs before its window records, so every site of the
    /// profiler is registered and the caches are warm.
    inline constexpr std::uint32_t K_WARMUP_PERIODS = 20;

    /// @brief Holds the periods a case records as active; 2000 make p99 the twentieth largest.
    inline constexpr std::uint32_t K_ACTIVE_PERIODS = 2000;

    /// @brief One timed case: what one output sample costs, and what share of one core that
    /// would take to keep up in real time at the reference sample rate.
    struct Result
    {
            const char* name;
            double nanoseconds_per_sample;
            double realtime_percent;

            /// @brief Stores the 99th percentile cost per sample; 0 when the case was not timed.
            double p99_nanoseconds_per_sample = 0.0;

            /// @brief Stores the largest cost per sample; 0 when the case was not timed.
            double maximum_nanoseconds_per_sample = 0.0;

            /// @brief Stores the median absolute difference between successive repetitions, in
            /// nanoseconds per call.
            double jitter_nanoseconds = 0.0;

            /// @brief Stores how many repetitions the figures come from.
            std::size_t periods = 0;

            /// @brief Stores the budget of one call, in nanoseconds: its samples over the rate.
            double budget_nanoseconds = 0.0;

            /// @brief Stores the profiler's record of the run; empty when the profiler was not
            /// running, in which case no check can be judged.
            profile::ProfileRecord record;

            /// @brief Converts the per-sample cost to milliseconds for a block of the given size,
            /// i.e. what one `process()` call on that block actually costs the audio thread.
            /// @param block Block size in frames.
            /// @return Milliseconds spent processing one block of that size.
            double milliseconds_per_block(std::uint32_t block) const
            {
                return nanoseconds_per_sample * block / 1e6;
            }
    };

    /// @brief Builds a Result from the time of each repetition: the median, p99 and maximum.
    /// @param call_ns The time of each repetition, in nanoseconds; not empty.
    inline Result result_from_calls(const char* name, std::uint32_t samples_per_call,
                                    double sample_rate, std::vector<double> call_ns)
    {
        std::sort(call_ns.begin(), call_ns.end());
        const std::size_t count = call_ns.size();
        auto percentile = [&call_ns, count](double fraction)
        {
            const double rank = fraction * static_cast<double>(count - 1);
            const auto low = static_cast<std::size_t>(std::floor(rank));
            const std::size_t high = std::min(low + 1, count - 1);
            return call_ns[low] +
                   (rank - static_cast<double>(low)) * (call_ns[high] - call_ns[low]);
        };
        const double per_sample = percentile(0.5) / samples_per_call;
        Result result{name, per_sample, 100.0 * per_sample / (1e9 / sample_rate)};
        result.p99_nanoseconds_per_sample = percentile(0.99) / samples_per_call;
        result.maximum_nanoseconds_per_sample = call_ns.back() / samples_per_call;
        result.periods = count;
        result.budget_nanoseconds = 1e9 * samples_per_call / sample_rate;
        return result;
    }

    /// @brief Runs `body` repeatedly under the steady clock, with no profiler, and reports the
    /// median cost per sample. It is the path `measure` takes when no session can run, and it
    /// records nothing, so no check can be judged against its result.
    template <typename Body>
    Result measure_unrecorded(const char* name, std::uint32_t samples_per_call, double sample_rate,
                              Body&& body)
    {
        using clock = std::chrono::steady_clock;
        for (std::uint32_t i = 0; i < K_WARMUP_PERIODS; ++i)
        {
            body();
        }
        std::vector<double> call_ns;
        call_ns.reserve(K_ACTIVE_PERIODS);
        for (std::uint32_t i = 0; i < K_ACTIVE_PERIODS; ++i)
        {
            const auto start = clock::now();
            body();
            const auto stop = clock::now();
            call_ns.push_back(std::chrono::duration<double, std::nano>(stop - start).count());
        }
        Result result = result_from_calls(name, samples_per_call, sample_rate, call_ns);
        std::vector<double> differences;
        for (std::size_t i = 1; i < call_ns.size(); ++i)
        {
            differences.push_back(std::fabs(call_ns[i] - call_ns[i - 1]));
        }
        std::sort(differences.begin(), differences.end());
        result.jitter_nanoseconds = differences[differences.size() / 2];
        return result;
    }

    /// @brief Runs `body` in 20 warm-up and 2000 active periods of the timeline `bench.block`,
    /// each with the budget of one call, and reports the median cost per sample with its record.
    /// Without a session it falls back to measure_unrecorded.
    /// @param samples_per_call Output samples produced by one invocation of `body`.
    /// @param sample_rate Reference rate for the real-time budget column.
    /// @param body Callable invoked once per repetition; must not allocate.
    /// @see modules/foundation/benchmark_harness/README.md
    template <typename Body>
    Result measure(const char* name, std::uint32_t samples_per_call, double sample_rate,
                   Body&& body)
    {
        const double budget = 1e9 * samples_per_call / sample_rate;
        const profile::ProfiledRun run =
            profile::run_profiled(profile::names::K_BENCH_TIMELINE, K_WARMUP_PERIODS,
                                  K_ACTIVE_PERIODS, static_cast<std::uint64_t>(budget), body);
        if (!run.start.started())
        {
            return measure_unrecorded(name, samples_per_call, sample_rate, body);
        }
        const profile::TimelineSummary summary =
            run.record.summarise(profile::names::K_BENCH_TIMELINE);
        const double per_sample = summary.median_ns / samples_per_call;
        Result result{name, per_sample, 100.0 * per_sample / (1e9 / sample_rate)};
        result.p99_nanoseconds_per_sample = summary.p99_ns / samples_per_call;
        result.maximum_nanoseconds_per_sample = summary.maximum_ns / samples_per_call;
        result.jitter_nanoseconds = summary.jitter_ns;
        result.periods = summary.periods;
        result.budget_nanoseconds = budget;
        result.record = run.record;
        return result;
    }

    /// @brief Prints one table row for a Result, with its ms/256-frame-block cost and, when
    /// given, the real-time budget that block is allowed against.
    /// @param result The row to print.
    /// @param budget_milliseconds_per_256 Real-time budget for a 256-frame block, in
    /// milliseconds; 0.0 (the default) omits the budget from the printed line.
    inline void print(const Result& result, double budget_milliseconds_per_256 = 0.0)
    {
        std::printf("%-34s %10.1f ns/sample %8.2f %% of one core  %6.3f ms/256", result.name,
                    result.nanoseconds_per_sample, result.realtime_percent,
                    result.milliseconds_per_block(256));
        if (budget_milliseconds_per_256 > 0.0)
        {
            std::printf(" (budget %.2f)", budget_milliseconds_per_256);
        }
        if (result.periods != 0)
        {
            std::printf("  p99 %.3f max %.3f ms, jitter %.1f us",
                        result.p99_nanoseconds_per_sample * 256 / 1e6,
                        result.maximum_nanoseconds_per_sample * 256 / 1e6,
                        result.jitter_nanoseconds / 1e3);
        }
        std::printf("\n");
    }

    /// @brief Reads a file whole; empty when it cannot be opened.
    inline std::string read_checks_file(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    }

    /// @brief Prints the verdict of each check in a checks file against a Result's record.
    /// @param result The case measured.
    /// @param checks_path The checks file, in SushiProfiler's format.
    /// @param known_issue The KNOWN_ISSUES entry that explains a failure, or null; a failing
    /// check is then labelled with it.
    /// @return How many checks failed.
    inline std::size_t print_verdicts(const Result& result, const std::string& checks_path,
                                      const char* known_issue = nullptr)
    {
        if (result.record.empty())
        {
            std::printf("  checks %s: not judged, the profiler did not record this run\n",
                        checks_path.c_str());
            return 0;
        }
        const std::string text = read_checks_file(checks_path);
        if (text.empty())
        {
            std::printf("  checks %s: not judged, the file could not be read\n",
                        checks_path.c_str());
            return 0;
        }
        const profile::ProfileCheckReport report = result.record.check(text);
        if (!report.error.empty())
        {
            std::printf("  checks %s: refused: %s\n", checks_path.c_str(), report.error.c_str());
            return 0;
        }
        std::size_t failed = 0;
        for (const profile::ProfileVerdict& verdict : report.verdicts)
        {
            std::printf("  check %s %s %s: %.3f ms", verdict.passed ? "pass" : "FAIL",
                        verdict.subject.c_str(), verdict.statistic.c_str(), verdict.value / 1e6);
            if (!verdict.passed)
            {
                ++failed;
                std::printf(" (%s)", verdict.reason.c_str());
                if (known_issue != nullptr)
                {
                    std::printf(" [known issue %s]", known_issue);
                }
            }
            std::printf("\n");
        }
        return failed;
    }

    /// @brief Prints a row, its budget and the verdicts of its checks file.
    /// @param checks_relative The checks file, relative to the tests folder.
    /// @param known_issue The KNOWN_ISSUES entry behind a failing check, or null.
    /// @return How many checks failed; a benchmark never fails the run on it.
    inline std::size_t report(const Result& result, double budget_milliseconds_per_256,
                              const char* checks_relative, const char* known_issue = nullptr)
    {
        print(result, budget_milliseconds_per_256);
        return print_verdicts(result, std::string(SUSHIDSP_TESTS_DIR) + "/" + checks_relative,
                              known_issue);
    }
} // namespace SushiDSP::bench
