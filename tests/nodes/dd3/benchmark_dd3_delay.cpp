/**************************************************************************/
/* benchmark_dd3_delay.cpp                                                */
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
 * @file benchmark_dd3_delay.cpp
 * @brief Times Dd3Delay's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/dd3/dd3_delay.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_DD3_DELAY_CASE{
        "Dd3",
        []
        {
            SushiDSP::bench::report(SushiDSP::bench::measure_node<SushiDSP::Dd3Delay>(
                                       "DD-3 Delay",
                                       [](SushiDSP::Dd3Delay& node)
                                       {
                                        node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
                                        node.set_time(0.4f);
                                        node.set_feedback(0.45f);
                                        node.set_level(0.5f);
                                       }),
                                   0.15, "nodes/dd3/dd3_delay.checks.json", "KI-47");
        }};
} // namespace
