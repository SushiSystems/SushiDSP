/**************************************************************************/
/* benchmark_tube_screamer_circuits.cpp                                   */
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
 * @file benchmark_tube_screamer_circuits.cpp
 * @brief Times the Tube Screamer's clip block and tone block, each at 8x oversampling.
 * @author Mustafa Garip
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>
#include <SushiDSP/foundation/benchmark_harness/reference_tone.hpp>
#include <SushiDSP/foundation/benchmark_harness/solver_counters.hpp>
#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/nodes/tubescreamer/tube_screamer_circuits.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    // The Tube Screamer's blocks are the product module's own; see tests/nodes/tubescreamer/README.md.
    using namespace SushiDSP::tube_screamer_circuits;

    constexpr std::uint32_t K_OVERSAMPLE_FACTOR = 8;

    /// @brief Times one 256-frame block of the TS808 clip block run at 8x oversampling.
    ///
    /// There is no bias point to settle: the block sits at zero at DC. The oversampling loop is
    /// written out inline because the case prices the solver, and Oversampler8x has its own row.
    /// One block is 256 * 8 = 2048 solves, with the drive pot where the TubeScreamer case sets it.
    SushiDSP::bench::Result measure_ts_clip_block()
    {
        const double run_rate = K_SAMPLE_RATE * K_OVERSAMPLE_FACTOR;

        ClipNetlist netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit circuit;
        DiodePairPort port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("ts_clip prepare failed; case skipped\n");
            return {"TubeScreamerBlock clip @8x", 0.0, 0.0};
        }
        circuit.set_potentiometer(pot, drive_to_position(0.7));
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "TubeScreamerBlock clip @8x", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                std::array<double, 1> outputs{};
                for (std::uint32_t frame = 0; frame < K_BLOCK_SIZE; ++frame)
                {
                    const std::array<double, 1> inputs{
                        static_cast<double>(input[frame]) *
                        static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS)};
                    for (std::uint32_t step = 0; step < K_OVERSAMPLE_FACTOR; ++step)
                    {
                        circuit.process_sample(port, inputs, outputs);
                    }
                    output[frame] = static_cast<float>(outputs[0]);
                }
            });

        SushiDSP::bench::print_solver_counters(circuit);
        return result;
    }

    /// @brief Times one 256-frame block of the TS808 tone and level block at 8x oversampling.
    ///
    /// The block has no nonlinear branch, so LinearPort is what it is stepped with and the cost
    /// is the state-space update alone. Its counters are printed anyway, because a zero
    /// non-convergence rate on an N = 0 block is what says the shape really is linear.
    SushiDSP::bench::Result measure_ts_tone_block()
    {
        const double run_rate = K_SAMPLE_RATE * K_OVERSAMPLE_FACTOR;

        ToneNetlist netlist;
        const TonePotentiometers pots = build_tone_block(netlist);

        ToneCircuit circuit;
        LinearPort port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("ts_tone prepare failed; case skipped\n");
            return {"TubeScreamerBlock tone @8x", 0.0, 0.0};
        }
        circuit.set_potentiometer(pots.tone, tone_to_position(0.5));
        circuit.set_potentiometer(pots.level, K_LEVEL_POT_POSITION);
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "TubeScreamerBlock tone @8x", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                std::array<double, 1> outputs{};
                for (std::uint32_t frame = 0; frame < K_BLOCK_SIZE; ++frame)
                {
                    const std::array<double, 1> inputs{
                        static_cast<double>(input[frame]) *
                        static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS)};
                    for (std::uint32_t step = 0; step < K_OVERSAMPLE_FACTOR; ++step)
                    {
                        circuit.process_sample(port, inputs, outputs);
                    }
                    output[frame] = static_cast<float>(outputs[0]);
                }
            });

        SushiDSP::bench::print_solver_counters(circuit);
        return result;
    }

    const SushiDSP::bench::BenchmarkRegistration K_TUBE_SCREAMER_BLOCK_CASE{
        "TubeScreamerBlock",
        []
        {
            // No budget here: the 0.25 ms line belongs to the TubeScreamer node these two feed.
            SushiDSP::bench::print(measure_ts_clip_block());
            SushiDSP::bench::print(measure_ts_tone_block());
        }};
} // namespace
