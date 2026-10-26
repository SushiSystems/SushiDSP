/**************************************************************************/
/* benchmark_jcm800.cpp                                                   */
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
 * @file benchmark_jcm800.cpp
 * @brief Times Jcm800's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/jcm800/jcm800.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_JCM800_CASE{
        "Jcm800",
        []
        {
            SushiDSP::bench::report(SushiDSP::bench::measure_node<SushiDSP::Jcm800>(
                                       "Jcm800 (no cabinet)",
                                       [](SushiDSP::Jcm800& node)
                                       {
                                        node.set_preamp(0.6f);
                                        node.set_master(0.0f);
                                        node.set_bass(0.6f);
                                        node.set_middle(0.4f);
                                        node.set_treble(0.7f);
                                        node.set_presence(0.5f);
                                       }),
                                   1.5, "nodes/jcm800/jcm800.checks.json");
        }};
} // namespace
