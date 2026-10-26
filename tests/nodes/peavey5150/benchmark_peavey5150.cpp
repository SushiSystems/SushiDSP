/**************************************************************************/
/* benchmark_peavey5150.cpp                                               */
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
 * @file benchmark_peavey5150.cpp
 * @brief Times Peavey5150's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/peavey5150/peavey5150.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_PEAVEY5150_CASE{
        "Peavey5150",
        []
        {
            SushiDSP::bench::report(SushiDSP::bench::measure_node<SushiDSP::Peavey5150>(
                                       "Peavey5150 (no cabinet)",
                                       [](SushiDSP::Peavey5150& node)
                                       {
                                        node.set_parameter_for_control_thread("pre_gain", 0.7f);
                                        node.set_parameter_for_control_thread("low", 0.5f);
                                        node.set_parameter_for_control_thread("mid", 0.5f);
                                        node.set_parameter_for_control_thread("high", 0.7f);
                                        node.set_parameter_for_control_thread("presence", 0.5f);
                                       }),
                                   1.5, "nodes/peavey5150/peavey5150.checks.json");
        }};
} // namespace
