/**************************************************************************/
/* Benchmark.hpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                        */
/**************************************************************************/

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
    };

    /// @brief Runs `body` repeatedly and reports the fastest observed cost per sample.
    ///
    /// The minimum, not the mean, is reported: on a desktop OS the variance is scheduler noise
    /// stacked on the true cost, so the minimum is the least-contaminated estimate of what the
    /// audio thread actually pays.
    /// @param name Case label printed in the table.
    /// @param samples_per_call Output samples produced by one invocation of `body`.
    /// @param sample_rate Reference rate for the real-time budget column.
    /// @param body Callable invoked once per repetition; must not allocate.
    /// @return The measured Result.
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

    /// @brief Prints one table row for a Result.
    /// @param result The row to print.
    inline void print(const Result& result)
    {
        std::printf("%-34s %10.1f ns/sample %8.2f %% of one core\n",
            result.name, result.nanoseconds_per_sample, result.realtime_percent);
    }
}
