/**************************************************************************/
/* node_benchmark.hpp                                                     */
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
 * @file node_benchmark.hpp
 * @brief Declares the case that times one node's process() the way a device callback calls it.
 * @author Mustafa Garip
 */

#pragma once

#include <vector>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>

namespace SushiDSP::bench
{
    /// @brief Times one mono node's process() over one block of the reference tone.
    /// @tparam Node A default-constructible AudioNode with one input and one output channel.
    /// @param name The row's name; it outlives the call.
    /// @param configure Called once with the prepared node, before the timing starts.
    template <typename Node, typename Configure>
    Result measure_node(const char* name, Configure&& configure)
    {
        Node node;
        node.prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 1});
        configure(node);
        const std::vector<float> input = reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        return measure(name, K_BLOCK_SIZE, K_SAMPLE_RATE,
                       [&]
                       {
                           ProcessContext context{outputs, inputs, K_BLOCK_SIZE, 0.0};
                           node.process(context);
                       });
    }
} // namespace SushiDSP::bench
