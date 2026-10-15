/**************************************************************************/
/* benchmark_tube_screamer.cpp                                            */
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
 * @file benchmark_tube_screamer.cpp
 * @brief Times TubeScreamer's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_TUBE_SCREAMER_CASE{
        "TubeScreamer",
        []
        {
            SushiDSP::bench::print(SushiDSP::bench::measure_node<SushiDSP::TubeScreamer>(
                                       "TubeScreamer",
                                       [](SushiDSP::TubeScreamer& node)
                                       {
                                        node.set_drive(0.7f);
                                        node.set_tone(0.5f);
                                        node.set_level(0.8f);
                                       }),
                                   0.25);
        }};
} // namespace
