/**************************************************************************/
/* Benchmark.hpp                                                          */
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
 * @file Benchmark.hpp
 * @brief Defines the timing harness and the table row the benchmark cases print.
 * @author Mustafa Garip
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>

namespace SushiDSP::bench
{
    /// @brief One timed case: what one output sample costs, and what share of one core that
    /// would take to keep up in real time at the reference sample rate.
    struct Result
    {
        const char* name;
        double      nanoseconds_per_sample;
        double      realtime_percent;

        /// @brief Converts the per-sample cost to milliseconds for a block of the given size,
        /// i.e. what one `process()` call on that block actually costs the audio thread.
        /// @param block Block size in frames.
        /// @return Milliseconds spent processing one block of that size.
        double milliseconds_per_block(std::uint32_t block) const
        {
            return nanoseconds_per_sample * block / 1e6;
        }
    };

    /// @brief Runs `body` repeatedly and reports the fastest observed cost per sample.
    /// @param samples_per_call Output samples produced by one invocation of `body`.
    /// @param sample_rate Reference rate for the real-time budget column.
    /// @param body Callable invoked once per repetition; must not allocate.
    /// @see tests/benchmark/README.md, "Benchmark.hpp", for the choice of the minimum.
    template <typename Body>
    Result measure(const char* name, std::uint32_t samples_per_call, double sample_rate, Body&& body)
    {
        using clock = std::chrono::steady_clock;
        constexpr int warmup_repetitions = 20;
        constexpr int timed_repetitions = 200;
        for (int i = 0; i < warmup_repetitions; ++i)
        {
            body();
        }
        double best_nanoseconds = 1e300;
        for (int i = 0; i < timed_repetitions; ++i)
        {
            const auto start = clock::now();
            body();
            const auto stop = clock::now();
            const double nanoseconds = std::chrono::duration<double, std::nano>(stop - start).count();
            if (nanoseconds < best_nanoseconds)
            {
                best_nanoseconds = nanoseconds;
            }
        }
        const double per_sample = best_nanoseconds / samples_per_call;
        const double budget_nanoseconds = 1e9 / sample_rate;
        return { name, per_sample, 100.0 * per_sample / budget_nanoseconds };
    }

    /// @brief Prints one table row for a Result, with its ms/256-frame-block cost and, when
    /// given, the real-time budget that block is allowed against.
    /// @param result The row to print.
    /// @param budget_milliseconds_per_256 Real-time budget for a 256-frame block, in
    /// milliseconds; 0.0 (the default) omits the budget from the printed line.
    inline void print(const Result& result, double budget_milliseconds_per_256 = 0.0)
    {
        std::printf("%-34s %10.1f ns/sample %8.2f %% of one core  %6.3f ms/256",
            result.name, result.nanoseconds_per_sample, result.realtime_percent,
            result.milliseconds_per_block(256));
        if (budget_milliseconds_per_256 > 0.0)
        {
            std::printf(" (budget %.2f)", budget_milliseconds_per_256);
        }
        std::printf("\n");
    }
}
