/**************************************************************************/
/* benchmark_resampler.cpp                                                */
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
 * @file benchmark_resampler.cpp
 * @brief Times one block through the DD-3's two resamplers at the slow clock.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/math/resampler.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Times one block down to the 20 kHz clock and back.
    /// @see tests/foundation/math/README.md for the worst-case clock and the case's limits.
    SushiDSP::bench::Result measure_resampler_round_trip()
    {
        constexpr double K_SLOW_CLOCK_HZ = 20000.0;

        SushiDSP::Resampler down;
        SushiDSP::Resampler up;
        down.prepare(K_SAMPLE_RATE, K_SLOW_CLOCK_HZ, K_BLOCK_SIZE, K_SLOW_CLOCK_HZ / K_SAMPLE_RATE);
        const std::uint32_t clock_capacity = down.maximum_output_frames(K_BLOCK_SIZE) + 8u;
        up.prepare(K_SLOW_CLOCK_HZ, K_SAMPLE_RATE, clock_capacity, K_SAMPLE_RATE / 82500.0);
        const std::uint32_t base_capacity = up.maximum_output_frames(clock_capacity) + 8u;

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> at_clock_rate(clock_capacity, 0.0f);
        std::vector<float> back_at_base_rate(base_capacity, 0.0f);

        return SushiDSP::bench::measure(
            "Resampler 48k-20k-48k round trip", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                const std::uint32_t produced =
                    down.process(input.data(), K_BLOCK_SIZE, at_clock_rate.data(), clock_capacity);
                up.process(at_clock_rate.data(), produced, back_at_base_rate.data(), base_capacity);
            });
    }

    const SushiDSP::bench::BenchmarkRegistration K_RESAMPLER_CASE{
        "Resampler",
        []
        {
            SushiDSP::bench::report(measure_resampler_round_trip(), 0.10,
                                    "foundation/math/resampler.checks.json");
        }};
} // namespace
