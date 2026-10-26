/**************************************************************************/
/* benchmark_image_source_solver.cpp                                      */
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
 * @file benchmark_image_source_solver.cpp
 * @brief Times one ImageSourceSolver::solve of the garage, the largest share of a room publish.
 * @author Mustafa Garip
 */

#include <cstdint>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/nodes/room/early_tap_set.hpp>
#include <SushiDSP/nodes/room/image_source_solver.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

namespace
{
    /// @brief Times one solve of the garage with the default options.
    SushiDSP::bench::Result measure_image_source_solve()
    {
        const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
        std::uint32_t taps = 0;
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "ImageSourceSolver::solve", SushiDSP::bench::K_BLOCK_SIZE,
            SushiDSP::bench::K_SAMPLE_RATE,
            [&]
            {
                taps += SushiDSP::room::ImageSourceSolver::solve(garage, {}).count;
            });
        static_cast<void>(taps);
        return result;
    }

    const SushiDSP::bench::BenchmarkRegistration K_IMAGE_SOURCE_SOLVER_CASE{
        "RoomImageSourceSolver",
        []
        {
            SushiDSP::bench::report(measure_image_source_solve(), 25.0,
                                    "nodes/room/image_source_solver.checks.json");
        }};
} // namespace
