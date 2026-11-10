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
    /// @brief Times one mono node's process() over one block of the reference tone, on a node
    /// the caller built.
    /// @param node The node to time; it is prepared here and outlives the call.
    /// @param name The row's name; it outlives the call.
    /// @param configure Called once with the prepared node, before the timing starts.
    template <typename Node, typename Configure>
    Result measure_node(Node& node, const char* name, Configure&& configure)
    {
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

    /// @brief Times one mono node's process() over one block of the reference tone.
    /// @tparam Node A default-constructible AudioNode with one input and one output channel.
    /// @param name The row's name; it outlives the call.
    /// @param configure Called once with the prepared node, before the timing starts.
    template <typename Node, typename Configure>
    Result measure_node(const char* name, Configure&& configure)
    {
        Node node;
        return measure_node(node, name, configure);
    }
} // namespace SushiDSP::bench
