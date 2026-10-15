/**************************************************************************/
/* benchmark_early_reflection_renderer.cpp                                */
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
 * @file benchmark_early_reflection_renderer.cpp
 * @brief Times EarlyReflectionRenderer::render alone, fed the garage's own taps.
 * @author Mustafa Garip
 */

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/early_reflection_renderer.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/room_render_set.hpp>
#include <SushiDSP/nodes/room/spherical_head_hrtf.hpp>

#include "garage_benchmark_scene.hpp"

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    /// @brief Times render() for one listener kind, so the taps' own cost is read apart from the
    /// node's total: sixteen channels of encode for the ears, one for a microphone.
    /// @param name The row's name; it outlives the call.
    SushiDSP::bench::Result measure_early_reflection_renderer(const char* name,
                                                              SushiDSP::room::ListenerKind listener)
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
            SushiDSP::test::garage_benchmark_scene(), hrtf, K_SAMPLE_RATE, listener);
        std::printf("%s renders %u taps\n", name, static_cast<unsigned>(set->early.count));

        SushiDSP::room::EarlyReflectionRenderer renderer;
        constexpr std::uint32_t K_MAX_DELAY_FRAMES = 8397;
        renderer.prepare(K_MAX_DELAY_FRAMES, K_BLOCK_SIZE);
        SushiDSP::room::DelayRing ring;
        ring.prepare(K_MAX_DELAY_FRAMES, K_BLOCK_SIZE);

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> channels(
            SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS * K_BLOCK_SIZE, 0.0f);
        std::array<float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
            channel_pointers{};
        for (std::uint32_t channel = 0;
             channel < SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS; ++channel)
        {
            channel_pointers[channel] = channels.data() + channel * K_BLOCK_SIZE;
        }

        ring.write(input.data(), K_BLOCK_SIZE);
        return SushiDSP::bench::measure(
            name, K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                renderer.render(ring, set->early, 0, channel_pointers.data(), K_BLOCK_SIZE);
            });
    }

    const SushiDSP::bench::BenchmarkRegistration K_EARLY_REFLECTION_RENDERER_CASE{
        "RoomEarlyReflectionRenderer",
        []
        {
            SushiDSP::bench::print(measure_early_reflection_renderer(
                "EarlyReflectionRenderer::render", SushiDSP::room::ListenerKind::Ears));
            SushiDSP::bench::print(
                measure_early_reflection_renderer("EarlyReflectionRenderer::render (microphone)",
                                                  SushiDSP::room::ListenerKind::Microphone));
        }};
} // namespace
