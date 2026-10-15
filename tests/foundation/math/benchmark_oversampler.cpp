/**************************************************************************/
/* benchmark_oversampler.cpp                                              */
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
 * @file benchmark_oversampler.cpp
 * @brief Times one Oversampler8x round trip over one block.
 * @author Mustafa Garip
 */

#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/math/oversampler.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    const SushiDSP::bench::BenchmarkRegistration K_OVERSAMPLER_CASE{
        "Oversampler8x",
        []
        {
            SushiDSP::Oversampler8x oversampler(K_BLOCK_SIZE);
            const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
            std::vector<float> output(K_BLOCK_SIZE, 0.0f);
            SushiDSP::bench::print(SushiDSP::bench::measure(
                "Oversampler8x round trip", K_BLOCK_SIZE, K_SAMPLE_RATE,
                [&]
                {
                    const float* upsampled = oversampler.upsample(input.data(), K_BLOCK_SIZE);
                    oversampler.downsample(upsampled, K_BLOCK_SIZE * 8, output.data());
                }));
        }};
} // namespace
