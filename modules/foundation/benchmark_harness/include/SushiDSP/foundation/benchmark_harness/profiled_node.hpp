/**************************************************************************/
/* profiled_node.hpp                                                      */
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
 * @file profiled_node.hpp
 * @brief Declares the run that processes one block of the reference tone per profiler period.
 * @author Mustafa Garip
 */

#pragma once

#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/profile/profile_run.hpp>

namespace SushiDSP::bench
{
    /// @brief Processes one block of the reference tone through a prepared mono node, once per
    /// period of the `bench.block` timeline, and returns the record.
    /// @tparam Node A default-constructible AudioNode with one input and one output channel.
    /// @param warmup The periods recorded and marked warm-up.
    /// @param active The periods recorded as active.
    template <typename Node>
    profile::ProfiledRun profile_node(std::uint32_t warmup, std::uint32_t active)
    {
        Node node;
        node.prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 1});
        const std::vector<float> input = reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        return profile::run_profiled(profile::names::K_BENCH_TIMELINE, warmup, active, 0U,
                                     [&]
                                     {
                                         ProcessContext context{outputs, inputs, K_BLOCK_SIZE,
                                                                0.0};
                                         node.process(context);
                                     });
    }
} // namespace SushiDSP::bench
