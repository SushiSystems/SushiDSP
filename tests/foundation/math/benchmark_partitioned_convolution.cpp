/**************************************************************************/
/* benchmark_partitioned_convolution.cpp                                  */
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
 * @file benchmark_partitioned_convolution.cpp
 * @brief Times one PartitionedConvolution block against a 4096-sample impulse response.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/math/partitioned_convolution.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    const SushiDSP::bench::BenchmarkRegistration K_PARTITIONED_CONVOLUTION_CASE{
        "PartitionedConvolution",
        []
        {
            constexpr std::uint32_t K_IMPULSE_LENGTH = 4096;
            std::vector<float> impulse(K_IMPULSE_LENGTH);
            for (std::uint32_t i = 0; i < K_IMPULSE_LENGTH; ++i)
            {
                impulse[i] = std::exp(-static_cast<float>(i) / 1000.0f) *
                             static_cast<float>(std::sin(0.1 * static_cast<double>(i)));
            }
            SushiDSP::PartitionedConvolution convolution(K_BLOCK_SIZE, impulse.data(),
                                                         K_IMPULSE_LENGTH);
            const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
            std::vector<float> output(K_BLOCK_SIZE, 0.0f);
            SushiDSP::bench::print(SushiDSP::bench::measure(
                "PartitionedConvolution (4096 IR)", K_BLOCK_SIZE, K_SAMPLE_RATE,
                [&] { convolution.process(input.data(), output.data()); }));
        }};
} // namespace
