/**************************************************************************/
/* benchmark_diezel_vh4.cpp                                               */
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
 * @file benchmark_diezel_vh4.cpp
 * @brief Times DiezelVh4's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/diezel_vh4/diezel_vh4.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_DIEZEL_VH4_CASE{
        "DiezelVh4",
        []
        {
            SushiDSP::bench::print(SushiDSP::bench::measure_node<SushiDSP::DiezelVh4>(
                                       "DiezelVh4 (no cabinet)",
                                       [](SushiDSP::DiezelVh4& node)
                                       {
                                        node.set_parameter_for_control_thread("gain", 0.7f);
                                        node.set_parameter_for_control_thread("bass", 0.5f);
                                        node.set_parameter_for_control_thread("middle", 0.5f);
                                        node.set_parameter_for_control_thread("treble", 0.7f);
                                        node.set_parameter_for_control_thread("presence", 0.5f);
                                       }),
                                   1.5);
        }};
} // namespace
