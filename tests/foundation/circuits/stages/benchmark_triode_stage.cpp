/**************************************************************************/
/* benchmark_triode_stage.cpp                                             */
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
 * @file benchmark_triode_stage.cpp
 * @brief Times one block of the V1a TriodeStage at the base rate.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/circuits/stages/triode_stage.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    const SushiDSP::bench::BenchmarkRegistration K_TRIODE_STAGE_CASE{
        "TriodeStage",
        []
        {
            // The 12AX7 values Jcm800 uses for V1a, on a 194 / 115 V plate swing.
            SushiDSP::TriodeStage triode(10000.0f, 1000000.0f, -1.5f, 60.0f,
                                         SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                                         SushiDSP::PlateSwing{194.0f, 115.0f});
            const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
            std::vector<float> output(K_BLOCK_SIZE, 0.0f);
            SushiDSP::bench::print(SushiDSP::bench::measure(
                "TriodeStage (base rate)", K_BLOCK_SIZE, K_SAMPLE_RATE,
                [&]
                {
                    for (std::uint32_t i = 0; i < K_BLOCK_SIZE; ++i)
                    {
                        output[i] = triode.process_sample(input[i]);
                    }
                }));
        }};
} // namespace
