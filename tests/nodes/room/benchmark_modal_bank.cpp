/**************************************************************************/
/* benchmark_modal_bank.cpp                                               */
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
 * @file benchmark_modal_bank.cpp
 * @brief Times ModalBank::process alone, fed the garage's own resonator design.
 * @author Mustafa Garip
 */

#include <memory>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/modal_bank.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#include "garage_benchmark_scene.hpp"

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Times ModalBank::process over one block of the garage's design. Plan Task 29 step 4.
    SushiDSP::bench::Result measure_modal_bank()
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const std::unique_ptr<SushiDSP::room::RoomRenderSet> set =
            SushiDSP::room::build_render_set(SushiDSP::test::garage_benchmark_scene(), hrtf,
                                             K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);

        SushiDSP::room::ModalBank bank;
        bank.prepare(K_BLOCK_SIZE);

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        return SushiDSP::bench::measure("ModalBank::process", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            bank.process(set->modal, input.data(), left.data(),
                                                         right.data(), K_BLOCK_SIZE);
                                        });
    }

    const SushiDSP::bench::BenchmarkRegistration K_MODAL_BANK_CASE{
        "RoomModalBank", [] { SushiDSP::bench::print(measure_modal_bank()); }};
} // namespace
