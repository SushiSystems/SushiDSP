/**************************************************************************/
/* benchmark_dk_circuit.cpp                                               */
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
 * @file benchmark_dk_circuit.cpp
 * @brief Times the JCM800 V1a block on DkCircuit: a solve, a pot update and a re-derivation.
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
#include <SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp>
#include <SushiDSP/foundation/core/calibration.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>

namespace
{
    using SushiDSP::bench::K_BLOCK_SIZE;
    using SushiDSP::bench::K_SAMPLE_RATE;

    // build_jcm_v1a(netlist, true) yields the seven-state block; see tests/foundation/math/dk/README.md.
    using namespace SushiDSP::test_circuits;

    constexpr std::uint32_t K_OVERSAMPLE_FACTOR = 8;

    /// @brief The benchmarked shape: seven states, two nonlinear branches, two sources, one probe.
    using DkStageCircuit = SushiDSP::dk::DkCircuit<7, 2, 2, 1>;
    using DkStageNetlist = SushiDSP::dk::DkNetlist<7, 2, 2, 1>;

    /// @brief Runs the block on silence until its bias point has settled.
    /// @param circuit The prepared circuit; advanced in place.
    /// @param port The triode glue.
    /// @param run_rate Rate the circuit was prepared at, in hertz.
    void settle_dk_stage(DkStageCircuit& circuit, TriodeStagePort& port, double run_rate)
    {
        const std::array<double, 2> silence{0.0, K_SUPPLY_VOLTS};
        std::array<double, 1> outputs{};
        const std::size_t samples = static_cast<std::size_t>(0.2 * run_rate);
        for (std::size_t index = 0; index < samples; ++index)
        {
            circuit.process_sample(port, silence, outputs);
        }
    }

    /// @brief Times one 256-frame block of the DK stage run at 8x oversampling.
    ///
    /// The oversampling loop is written out inline rather than routed through Oversampler8x: the
    /// case exists to price the solver, and the polyphase filters would only blur that number.
    /// One block is 256 * 8 = 2048 solver calls.
    SushiDSP::bench::Result measure_dk_circuit()
    {
        const double run_rate = K_SAMPLE_RATE * K_OVERSAMPLE_FACTOR;

        DkStageNetlist netlist;
        build_jcm_v1a(netlist, true);

        DkStageCircuit circuit;
        TriodeStagePort port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return {"DkCircuit 2-nonlin 7-state @8x", 0.0, 0.0};
        }
        settle_dk_stage(circuit, port, run_rate);
        // The counters cover only the solves measure() drives; see tests/foundation/math/dk/README.md.
        circuit.reset_statistics();

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "DkCircuit 2-nonlin 7-state @8x", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                std::array<double, 1> outputs{};
                for (std::uint32_t frame = 0; frame < K_BLOCK_SIZE; ++frame)
                {
                    const double drive = static_cast<double>(input[frame]) *
                                         static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS);
                    const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
                    for (std::uint32_t step = 0; step < K_OVERSAMPLE_FACTOR; ++step)
                    {
                        circuit.process_sample(port, inputs, outputs);
                    }
                    output[frame] = static_cast<float>(outputs[0]);
                }
            });

        // The buffer wrap costs no solve, so this count is zero; see tests/foundation/math/dk/README.md.
        SushiDSP::bench::print_solver_counters(circuit);
        return result;
    }

    /// @brief Times one block in which the potentiometer moves, forcing a matrix re-derivation.
    ///
    /// Plan Ruling 3 lets a pot move re-derive the DK matrices on the audio thread at block
    /// cadence. This case is what makes that a measured cost rather than an estimate: the gap
    /// between it and the base-rate solve is the re-derivation, paid once per block.
    SushiDSP::bench::Result measure_dk_potentiometer_update()
    {
        DkStageNetlist netlist;
        const std::size_t pot_index = build_jcm_v1a(netlist, true);

        DkStageCircuit circuit;
        TriodeStagePort port;
        if (!circuit.prepare(netlist, K_SAMPLE_RATE))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return {"DkCircuit pot update (1x)", 0.0, 0.0};
        }
        settle_dk_stage(circuit, port, K_SAMPLE_RATE);

        const std::vector<float> input = SushiDSP::bench::reference_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        double position = 0.5;
        return SushiDSP::bench::measure(
            "DkCircuit pot update (1x)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                position = position > 0.5 ? 0.4 : 0.6;
                circuit.set_potentiometer(pot_index, position);
                circuit.update_matrices_if_dirty();

                std::array<double, 1> outputs{};
                for (std::uint32_t frame = 0; frame < K_BLOCK_SIZE; ++frame)
                {
                    const double drive = static_cast<double>(input[frame]) *
                                         static_cast<double>(SushiDSP::K_FULL_SCALE_VOLTS);
                    const std::array<double, 2> inputs{drive, K_SUPPLY_VOLTS};
                    circuit.process_sample(port, inputs, outputs);
                    output[frame] = static_cast<float>(outputs[0]);
                }
            });
    }

    /// @brief Times the potentiometer re-derivation on its own, with no solving around it.
    /// @see tests/foundation/math/dk/README.md for what the block case cannot supply.
    SushiDSP::bench::Result measure_dk_potentiometer_rederivation()
    {
        DkStageNetlist netlist;
        const std::size_t pot_index = build_jcm_v1a(netlist, true);

        DkStageCircuit circuit;
        TriodeStagePort port;
        if (!circuit.prepare(netlist, K_SAMPLE_RATE))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return {"DkCircuit pot re-derivation", 0.0, 0.0};
        }
        settle_dk_stage(circuit, port, K_SAMPLE_RATE);

        double position = 0.5;
        return SushiDSP::bench::measure("DkCircuit pot re-derivation", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            for (std::uint32_t index = 0; index < K_BLOCK_SIZE;
                                                 ++index)
                                            {
                                                position = position > 0.5 ? 0.4 : 0.6;
                                                circuit.set_potentiometer(pot_index, position);
                                                circuit.update_matrices_if_dirty();
                                            }
                                        });
    }

    const SushiDSP::bench::BenchmarkRegistration K_DK_CIRCUIT_CASE{
        "DkCircuit",
        []
        {
            SushiDSP::bench::print(measure_dk_circuit(), 0.12);
            SushiDSP::bench::print(measure_dk_potentiometer_update());
            SushiDSP::bench::print(measure_dk_potentiometer_rederivation());
        }};
} // namespace
