/**************************************************************************/
/* benchmark_mesa_mark2c.cpp                                              */
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
 * @file benchmark_mesa_mark2c.cpp
 * @brief Times MesaMark2c's process() over one block.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/node_benchmark.hpp>
#include <SushiDSP/nodes/mesa_mark2c/mesa_mark2c.hpp>

namespace
{
    const SushiDSP::bench::BenchmarkRegistration K_MESA_MARK2C_CASE{
        "MesaMark2c",
        []
        {
            SushiDSP::bench::print(SushiDSP::bench::measure_node<SushiDSP::MesaMark2c>(
                                       "MesaMark2c (no cabinet)",
                                       [](SushiDSP::MesaMark2c& node)
                                       {
                                        node.set_parameter_for_control_thread("lead_drive", 0.7f);
                                        node.set_parameter_for_control_thread("bass", 0.5f);
                                        node.set_parameter_for_control_thread("middle", 0.5f);
                                        node.set_parameter_for_control_thread("treble", 0.7f);
                                        node.set_parameter_for_control_thread("presence", 0.5f);
                                       }),
                                   1.5);
        }};
} // namespace
