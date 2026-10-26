/**************************************************************************/
/* benchmark_room_render_set.cpp                                          */
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
 * @file benchmark_room_render_set.cpp
 * @brief Times one build_render_set of the garage, the whole of a publish after a scene change.
 * @author Mustafa Garip
 */

#include <memory>

#include <cstdint>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>
#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>

namespace
{
    /// @brief Times one build of the garage's render set for the ears.
    SushiDSP::bench::Result measure_build_render_set()
    {
        const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(SushiDSP::bench::K_SAMPLE_RATE);
        std::uint32_t taps = 0;
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "build_render_set", SushiDSP::bench::K_BLOCK_SIZE, SushiDSP::bench::K_SAMPLE_RATE,
            [&]
            {
                const std::unique_ptr<SushiDSP::room::RoomRenderSet> set =
                    SushiDSP::room::build_render_set(garage, hrtf, SushiDSP::bench::K_SAMPLE_RATE,
                                                     SushiDSP::room::ListenerKind::Ears);
                taps += set->early.count;
            });
        static_cast<void>(taps);
        return result;
    }

    const SushiDSP::bench::BenchmarkRegistration K_ROOM_RENDER_SET_CASE{
        "RoomRenderSet",
        []
        {
            SushiDSP::bench::report(measure_build_render_set(), 33.0,
                                    "nodes/room/room_render_set.checks.json");
        }};
} // namespace
