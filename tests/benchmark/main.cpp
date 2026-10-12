/**************************************************************************/
/* main.cpp                                                               */
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
 * @file main.cpp
 * @brief Runs the SushiDSP benchmark cases and prints one table row per case.
 * @author Mustafa Garip
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "benchmark.hpp"
#include "SushiDSP/core/calibration.hpp"
#include "SushiDSP/core/denormal_guard.hpp"
#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/capture/track_player.hpp"
#include "SushiDSP/capture/transport.hpp"
#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/dd3/dd3_delay.hpp"
#include "SushiDSP/jcm800/jcm800.hpp"
#include "SushiDSP/mesa_mark2c/mesa_mark2c.hpp"
#include "SushiDSP/peavey5150/peavey5150.hpp"
#include "SushiDSP/diezel_vh4/diezel_vh4.hpp"
#include "SushiDSP/circuits/stages/triode_stage.hpp"
#include "SushiDSP/math/oversampler.hpp"
#include "SushiDSP/math/partitioned_convolution.hpp"
#include "SushiDSP/math/resampler.hpp"
#include "SushiDSP/math/dk/dk_circuit.hpp"
#include "SushiDSP/math/dk/dk_netlist.hpp"
#include "SushiDSP/room/binaural_decoder.hpp"
#include "SushiDSP/room/early_reflection_renderer.hpp"
#include "SushiDSP/room/modal_bank.hpp"
#include "SushiDSP/room/room_node.hpp"
#include "SushiDSP/room/room_render_set.hpp"
#include "SushiDSP/room/spherical_head_hrtf.hpp"
#include "SushiDSP/tubescreamer/tube_screamer.hpp"
#include "SushiDSP/tubescreamer/tube_screamer_circuits.hpp"

#include "circuits/jcm_v1a_netlist.hpp"
#include "common/room_fixtures.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double K_SAMPLE_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK_SIZE = 256;

    /// @brief The 220 Hz tone the regression snapshots use, so the bench exercises the same path.
    std::vector<float> make_test_tone(std::uint32_t frames)
    {
        std::vector<float> tone(frames);
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            tone[i] = 0.25f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 *
                                                          static_cast<double>(i) / K_SAMPLE_RATE));
        }
        return tone;
    }

    /// @brief Times one node's process() over a block, the way a device callback calls it.
    template <typename Node, typename Configure>
    SushiDSP::bench::Result measure_node(const char* name, Configure&& configure)
    {
        Node node;
        node.prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 1});
        configure(node);
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        return SushiDSP::bench::measure(name, K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            SushiDSP::ProcessContext context{outputs, inputs,
                                                                             K_BLOCK_SIZE, 0.0};
                                            node.process(context);
                                        });
    }

    // build_jcm_v1a(netlist, true) yields the seven-state block; see tests/benchmark/README.md.
    using namespace SushiDSP::test_circuits;

    // The Tube Screamer's blocks are the product module's own; see tests/benchmark/README.md.
    using namespace SushiDSP::tube_screamer_circuits;

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

    /// @brief Prints one DK circuit's solve, non-convergence and worst-iteration counters.
    ///
    /// A solver's cost and its convergence are the same measurement: a block that got fast by
    /// giving up early is not faster. Read the rate with the harness in mind - measure() replays
    /// the same 256-frame buffer, so the tone jumps back to sample 0 once per repetition, and
    /// that step is the one thing here a real stream never does.
    /// @param circuit The circuit whose counters to report.
    template <typename Circuit> void print_solver_counters(const Circuit& circuit)
    {
        const std::uint64_t solves = circuit.total_samples();
        const std::uint64_t non_converged = circuit.non_converged_samples();
        std::printf(
            "  solver: %llu solves, %llu non-converged (%.4f %%), %u iterations worst "
            "case, %llu NaN fallbacks\n",
            static_cast<unsigned long long>(solves), static_cast<unsigned long long>(non_converged),
            solves == 0 ? 0.0
                        : 100.0 * static_cast<double>(non_converged) / static_cast<double>(solves),
            circuit.maximum_iterations_seen(),
            static_cast<unsigned long long>(circuit.nan_fallback_samples()));
    }

    /// @brief Times one 256-frame block of the DK stage run at 8x oversampling.
    ///
    /// The oversampling loop is written out inline rather than routed through Oversampler8x: the
    /// case exists to price the solver, and the polyphase filters would only blur that number.
    /// One block is 256 * 8 = 2048 solver calls.
    /// @return The measured Result, costed per output sample at the base rate.
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
        // The counters cover only the solves measure() drives; see tests/benchmark/README.md.
        circuit.reset_statistics();

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
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

        // The buffer wrap costs no solve, so this count is zero; see tests/benchmark/README.md.
        print_solver_counters(circuit);
        return result;
    }

    /// @brief Times one block in which the potentiometer moves, forcing a matrix re-derivation.
    ///
    /// Plan Ruling 3 lets a pot move re-derive the DK matrices on the audio thread at block
    /// cadence. This case is what makes that a measured cost rather than an estimate: the gap
    /// between it and the base-rate solve is the re-derivation, paid once per block.
    /// @return The measured Result, costed per output sample at the base rate.
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

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
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
    /// @return The measured Result, whose ns/sample column is ns per re-derivation.
    /// @see tests/benchmark/README.md, "main.cpp", for what the block case cannot supply.
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

    /// @brief Times one 256-frame block of the TS808 clip block run at 8x oversampling.
    ///
    /// There is no bias point to settle: the block sits at zero at DC. The oversampling loop is
    /// written out inline for the same reason measure_dk_circuit() writes it out - the case
    /// prices the solver, and Oversampler8x has its own row. One block is 256 * 8 = 2048 solves,
    /// with the drive pot where the TubeScreamer node case sets it.
    /// @return The measured Result, costed per output sample at the base rate.
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

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
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

        print_solver_counters(circuit);
        return result;
    }

    /// @brief Times one 256-frame block of the TS808 tone and level block at 8x oversampling.
    ///
    /// The block has no nonlinear branch, so LinearPort is what it is stepped with and the cost
    /// is the state-space update alone. Its counters are printed anyway, because a zero
    /// non-convergence rate on an N = 0 block is what says the shape really is linear.
    /// @return The measured Result, costed per output sample at the base rate.
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

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
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

        print_solver_counters(circuit);
        return result;
    }

    /// @brief Times one 256-frame block through the DD-3's two resamplers at the slow clock.
    /// @return The measured Result, costed per output sample at the base rate.
    /// @see tests/benchmark/README.md, "main.cpp", for the worst-case clock and the case's limits.
    SushiDSP::bench::Result measure_resampler_round_trip()
    {
        constexpr double slow_clock_hz = 20000.0;

        SushiDSP::Resampler down;
        SushiDSP::Resampler up;
        down.prepare(K_SAMPLE_RATE, slow_clock_hz, K_BLOCK_SIZE, slow_clock_hz / K_SAMPLE_RATE);
        const std::uint32_t clock_capacity = down.maximum_output_frames(K_BLOCK_SIZE) + 8u;
        up.prepare(slow_clock_hz, K_SAMPLE_RATE, clock_capacity, K_SAMPLE_RATE / 82500.0);
        const std::uint32_t base_capacity = up.maximum_output_frames(clock_capacity) + 8u;

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> at_clock_rate(clock_capacity, 0.0f);
        std::vector<float> back_at_base_rate(base_capacity, 0.0f);

        return SushiDSP::bench::measure(
            "Resampler 48k-20k-48k round trip", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                const std::uint32_t produced =
                    down.process(input.data(), K_BLOCK_SIZE, at_clock_rate.data(), clock_capacity);
                up.process(at_clock_rate.data(), produced, back_at_base_rate.data(), base_capacity);
            });
    }

    /// @brief With no arguments every case runs; otherwise a case runs when any argument is a
    /// substring of its name.
    bool selected(const char* name, int argc, char** argv)
    {
        if (argc < 2)
        {
            return true;
        }
        for (int i = 1; i < argc; ++i)
        {
            if (std::strstr(name, argv[i]) != nullptr)
            {
                return true;
            }
        }
        return false;
    }

    /// @brief Times eight stereo tracks mixed into one stereo block, the load spec section 10 puts
    /// on the audio thread when a whole session plays back.
    /// @return The measured Result.
    SushiDSP::bench::Result measure_eight_track_mix()
    {
        constexpr std::size_t K_TRACKS = SushiDSP::TrackPlayer::K_MAX_TRACKS;
        constexpr std::size_t K_TAKE_FRAMES = 48000;

        static SushiDSP::TrackPlayer player;
        static SushiDSP::Transport transport;
        for (std::size_t track = 0; track < K_TRACKS; ++track)
        {
            auto take = std::make_unique<SushiDSP::Take>(2u, K_SAMPLE_RATE, K_TAKE_FRAMES);
            for (std::uint32_t channel = 0; channel < 2u; ++channel)
            {
                float* samples = take->mutable_channel(channel);
                for (std::size_t frame = 0; frame < K_TAKE_FRAMES; ++frame)
                {
                    samples[frame] = 0.1f * static_cast<float>(std::sin(2.0 * M_PI * 220.0 *
                                                                        static_cast<double>(frame) /
                                                                        K_SAMPLE_RATE));
                }
            }
            player.set_take(track, std::move(take));
            player.set_pan(track, -1.0f + 0.25f * static_cast<float>(track));
        }

        static std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        static std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        float* outputs[2] = {left.data(), right.data()};

        return SushiDSP::bench::measure(
            "TrackPlayer (8 stereo tracks)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                std::fill(left.begin(), left.end(), 0.0f);
                std::fill(right.begin(), right.end(), 0.0f);
                player.process(outputs, 2u, K_BLOCK_SIZE, transport);
                transport.advance(K_BLOCK_SIZE);
                if (transport.position_frames() + K_BLOCK_SIZE >= K_TAKE_FRAMES)
                {
                    transport.seek(0);
                }
            });
    }

    /// @brief Returns the garage with the cabinet at (0.4, 0.5, 0.5) and the ears and an omni
    /// microphone together at (5.4, 3.4, 2.0), the pose the modal, early and tail layers are all
    /// driven at (`modules/nodes/room/README.md`, "The wave layer and the seam").
    SushiDSP::room::RoomScene garage_every_layer_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = {0.4, 0.5, 0.5};
        scene.listener_position = {5.4, 3.4, 2.0};
        scene.microphone_position = {5.4, 3.4, 2.0};
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns a prepared, full-wet RoomNode rendering the garage for one listener.
    /// @param listener 0.0f for the ears, 1.0f for the microphone.
    std::unique_ptr<SushiDSP::room::RoomNode> prepared_room_node(float listener)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->set_parameter_for_control_thread("mix", 1.0f);
        node->set_parameter_for_control_thread("listener", listener);
        node->prepare({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});
        std::string reason;
        if (!node->set_scene(garage_every_layer_scene(), reason))
        {
            std::printf("RoomNode set_scene failed: %s\n", reason.c_str());
        }
        return node;
    }

    /// @brief Times the steady-state ears block: the garage, no publish in flight, every layer
    /// running. Plan Task 29 step 1; spec §12 item 10 budgets 1.8 ms.
    SushiDSP::bench::Result measure_room_ears_steady_state()
    {
        auto node = prepared_room_node(0.0f);
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        return SushiDSP::bench::measure("RoomNode ears (steady state)", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            SushiDSP::ProcessContext context{outputs, inputs,
                                                                             K_BLOCK_SIZE, 0.0};
                                            node->process(context);
                                        });
    }

    /// @brief Times the ears block while a publish lands every other block, so the 512-frame
    /// raised-cosine fade never lapses and every rendered block runs both early-reflection
    /// banks. Plan Task 29 step 2; spec §12 item 10 budgets 2.8 ms.
    SushiDSP::bench::Result measure_room_ears_swap()
    {
        auto node = prepared_room_node(0.0f);
        SushiDSP::room::RoomScene alternate = garage_every_layer_scene();
        alternate.source_position.x += 0.05;
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        bool alternate_next = true;
        std::string reason;
        return SushiDSP::bench::measure(
            "RoomNode ears (swap every block)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                const SushiDSP::room::RoomScene& scene =
                    alternate_next ? alternate : garage_every_layer_scene();
                node->set_scene(scene, reason);
                alternate_next = !alternate_next;
                SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK_SIZE, 0.0};
                node->process(context);
                node->reclaim_rooms();
            });
    }

    /// @brief Times the microphone's steady-state block. Plan Task 29 step 3.
    SushiDSP::bench::Result measure_room_microphone_steady_state()
    {
        auto node = prepared_room_node(1.0f);
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {left.data(), right.data()};
        return SushiDSP::bench::measure(
            "RoomNode microphone (steady state)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK_SIZE, 0.0};
                node->process(context);
            });
    }

    /// @brief Times EarlyReflectionRenderer::render alone, fed the garage's own taps, so DE7's
    /// decision reads the heaviest brick's own cost rather than the node's total. Plan Task 29
    /// step 4.
    SushiDSP::bench::Result measure_early_reflection_renderer()
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
            garage_every_layer_scene(), hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);

        SushiDSP::room::EarlyReflectionRenderer renderer;
        constexpr std::uint32_t K_MAX_DELAY_FRAMES = 8397;
        renderer.prepare(K_MAX_DELAY_FRAMES, K_BLOCK_SIZE);

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> channels(
            SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS * K_BLOCK_SIZE, 0.0f);
        std::array<float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
            channel_pointers{};
        for (std::uint32_t channel = 0;
             channel < SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS; ++channel)
        {
            channel_pointers[channel] = channels.data() + channel * K_BLOCK_SIZE;
        }

        renderer.write(input.data(), K_BLOCK_SIZE);
        return SushiDSP::bench::measure(
            "EarlyReflectionRenderer::render", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                renderer.render(set->early, 0, channel_pointers.data(), K_BLOCK_SIZE);
            });
    }

    /// @brief Times ModalBank::process alone, fed the garage's own resonator design. Plan Task
    /// 29 step 4.
    SushiDSP::bench::Result measure_modal_bank()
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        const std::unique_ptr<SushiDSP::room::RoomRenderSet> set = SushiDSP::room::build_render_set(
            garage_every_layer_scene(), hrtf, K_SAMPLE_RATE, SushiDSP::room::ListenerKind::Ears);

        SushiDSP::room::ModalBank bank;
        bank.prepare(K_BLOCK_SIZE);

        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        return SushiDSP::bench::measure("ModalBank::process", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            bank.process(set->modal, input.data(), left.data(),
                                                         right.data(), K_BLOCK_SIZE);
                                        });
    }

    /// @brief Times BinauralDecoder::process alone; its cost is fixed by the decoder's order and
    /// does not depend on the scene (`binaural_decoder.hpp`). Plan Task 29 step 4.
    SushiDSP::bench::Result measure_binaural_decoder()
    {
        const SushiDSP::room::HrtfSet hrtf =
            SushiDSP::room::SphericalHeadHrtf::build(K_SAMPLE_RATE);
        SushiDSP::room::BinauralDecoder decoder;
        decoder.prepare(hrtf, K_SAMPLE_RATE, K_BLOCK_SIZE);

        const std::vector<float> tone = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> channels(
            SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS * K_BLOCK_SIZE, 0.0f);
        std::array<const float*, SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS>
            channel_pointers{};
        for (std::uint32_t channel = 0;
             channel < SushiDSP::room::EarlyReflectionRenderer::K_OUTPUT_CHANNELS; ++channel)
        {
            float* row = channels.data() + channel * K_BLOCK_SIZE;
            std::memcpy(row, tone.data(), K_BLOCK_SIZE * sizeof(float));
            channel_pointers[channel] = row;
        }
        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        return SushiDSP::bench::measure("BinauralDecoder::process", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                        [&]
                                        {
                                            decoder.process(channel_pointers.data(), left.data(),
                                                            right.data(), K_BLOCK_SIZE);
                                        });
    }

    /// @brief One AudioNode root that plays the 220 Hz test tone, so a RigGraph chain has a
    /// signal to carry without a host-supplied input buffer.
    class ToneSourceNode final : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo& info) override
            {
                tone_ = make_test_tone(info.max_block);
            }

            void process(SushiDSP::ProcessContext& context) override
            {
                std::memcpy(context.outputs[0], tone_.data(), context.frames * sizeof(float));
            }

            SushiDSP::PortLayout ports() const override
            {
                return {0, 1};
            }

            float get_parameter(const char*) const override
            {
                return 0.0f;
            }
            void set_parameter_for_control_thread(const char*, float) override {}
            void reset() override {}

        private:
            std::vector<float> tone_;
    };

    /// @brief Times the room's own share of one block in a JCM800-into-room `RigGraph`, the
    /// shape DE7's decision is made against. Plan Task 29 step 5; over 2.0 ms opens DE7.
    SushiDSP::bench::Result measure_room_beside_jcm800()
    {
        SushiDSP::RigGraph graph(K_BLOCK_SIZE, 8);
        auto source_handle = graph.add_node(std::make_unique<ToneSourceNode>());
        auto amp_handle = graph.add_node(std::make_unique<SushiDSP::Jcm800>());
        auto room_handle = graph.add_node(std::make_unique<SushiDSP::room::RoomNode>());
        graph.connect(source_handle, amp_handle);
        graph.connect(amp_handle, room_handle);
        graph.compile({K_SAMPLE_RATE, K_BLOCK_SIZE, 2});

        auto* amp = static_cast<SushiDSP::Jcm800*>(graph.node(amp_handle));
        amp->set_preamp(0.6f);
        amp->set_master(0.0f);
        amp->set_bass(0.6f);
        amp->set_middle(0.4f);
        amp->set_treble(0.7f);
        amp->set_presence(0.5f);

        auto* room = static_cast<SushiDSP::room::RoomNode*>(graph.node(room_handle));
        room->set_parameter_for_control_thread("mix", 1.0f);
        room->set_parameter_for_control_thread("listener", 0.0f);
        std::string reason;
        if (!room->set_scene(garage_every_layer_scene(), reason))
        {
            std::printf("RoomNode set_scene failed: %s\n", reason.c_str());
        }

        std::vector<float> left(K_BLOCK_SIZE, 0.0f);
        std::vector<float> right(K_BLOCK_SIZE, 0.0f);
        float* outputs[2] = {left.data(), right.data()};

        const SushiDSP::bench::Result whole_chain = SushiDSP::bench::measure(
            "JCM800 + RoomNode (whole chain)", K_BLOCK_SIZE, K_SAMPLE_RATE,
            [&]
            {
                SushiDSP::ProcessContext context{outputs, nullptr, K_BLOCK_SIZE, 0.0};
                graph.process(context);
            });
        SushiDSP::bench::print(whole_chain);

        const SushiDSP::bench::Result amp_alone =
            measure_node<SushiDSP::Jcm800>("Jcm800 alone (for subtraction)",
                                           [](SushiDSP::Jcm800& node)
                                           {
                                               node.set_preamp(0.6f);
                                               node.set_master(0.0f);
                                               node.set_bass(0.6f);
                                               node.set_middle(0.4f);
                                               node.set_treble(0.7f);
                                               node.set_presence(0.5f);
                                           });

        // The room's share is the chain less the amplifier alone; see tests/benchmark/README.md.
        const double room_share_ns =
            whole_chain.nanoseconds_per_sample - amp_alone.nanoseconds_per_sample;
        return {"RoomNode share of JCM800 + Room", room_share_ns,
                100.0 * room_share_ns / (1e9 / K_SAMPLE_RATE)};
    }
} // namespace

int main(int argc, char** argv)
{
    using SushiDSP::bench::print;
    // Same FPU mode the device callbacks run under, so the numbers match the real thread.
    SushiDSP::DenormalGuard denormal_guard;
    std::printf("SushiDSP benchmark  (%g Hz, %u-frame blocks, best of 200)\n\n", K_SAMPLE_RATE,
                K_BLOCK_SIZE);

    if (selected("Jcm800", argc, argv))
    {
        print(measure_node<SushiDSP::Jcm800>("Jcm800 (no cabinet)",
                                             [](SushiDSP::Jcm800& node)
                                             {
                                                 node.set_preamp(0.6f);
                                                 node.set_master(0.0f);
                                                 node.set_bass(0.6f);
                                                 node.set_middle(0.4f);
                                                 node.set_treble(0.7f);
                                                 node.set_presence(0.5f);
                                             }),
              1.5);
    }
    if (selected("MesaMark2c", argc, argv))
    {
        print(measure_node<SushiDSP::MesaMark2c>(
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
    }
    if (selected("Peavey5150", argc, argv))
    {
        print(measure_node<SushiDSP::Peavey5150>(
                  "Peavey5150 (no cabinet)",
                  [](SushiDSP::Peavey5150& node)
                  {
                      node.set_parameter_for_control_thread("pre_gain", 0.7f);
                      node.set_parameter_for_control_thread("low", 0.5f);
                      node.set_parameter_for_control_thread("mid", 0.5f);
                      node.set_parameter_for_control_thread("high", 0.7f);
                      node.set_parameter_for_control_thread("presence", 0.5f);
                  }),
              1.5);
    }
    if (selected("DiezelVh4", argc, argv))
    {
        print(measure_node<SushiDSP::DiezelVh4>(
                  "DiezelVh4 (no cabinet)",
                  [](SushiDSP::DiezelVh4& node)
                  {
                      node.set_parameter_for_control_thread("gain", 0.7f);
                      node.set_parameter_for_control_thread("bass", 0.5f);
                      node.set_parameter_for_control_thread("middle", 0.5f);
                      node.set_parameter_for_control_thread("treble", 0.7f);
                      node.set_parameter_for_control_thread("presence", 0.5f);
                  }),
              1.5);
    }
    if (selected("TubeScreamer", argc, argv))
    {
        print(measure_node<SushiDSP::TubeScreamer>("TubeScreamer",
                                                   [](SushiDSP::TubeScreamer& node)
                                                   {
                                                       node.set_drive(0.7f);
                                                       node.set_tone(0.5f);
                                                       node.set_level(0.8f);
                                                   }),
              0.25);
    }
    if (selected("Dd3", argc, argv))
    {
        print(measure_node<SushiDSP::Dd3Delay>("DD-3 Delay",
                                               [](SushiDSP::Dd3Delay& node)
                                               {
                                                   node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
                                                   node.set_time(0.4f);
                                                   node.set_feedback(0.45f);
                                                   node.set_level(0.5f);
                                               }),
              0.15);
    }
    if (selected("TrackPlayer", argc, argv))
    {
        // 256 frames at 48 kHz leave the audio thread 5.33 ms for everything in the block.
        print(measure_eight_track_mix(), 5.33);
    }
    if (selected("Resampler", argc, argv))
    {
        print(measure_resampler_round_trip(), 0.10);
    }
    if (selected("Oversampler8x", argc, argv))
    {
        SushiDSP::Oversampler8x oversampler(K_BLOCK_SIZE);
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        print(SushiDSP::bench::measure("Oversampler8x round trip", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                       [&]
                                       {
                                           const float* upsampled =
                                               oversampler.upsample(input.data(), K_BLOCK_SIZE);
                                           oversampler.downsample(upsampled, K_BLOCK_SIZE * 8,
                                                                  output.data());
                                       }));
    }
    if (selected("TriodeStage", argc, argv))
    {
        // The 12AX7 values Jcm800 uses for V1a, on a 194 / 115 V plate swing.
        SushiDSP::TriodeStage triode(10000.0f, 1000000.0f, -1.5f, 60.0f,
                                     SushiDSP::K_TRIODE12_AX7_GRID_CONDUCTION,
                                     SushiDSP::PlateSwing{194.0f, 115.0f});
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        print(SushiDSP::bench::measure("TriodeStage (base rate)", K_BLOCK_SIZE, K_SAMPLE_RATE,
                                       [&]
                                       {
                                           for (std::uint32_t i = 0; i < K_BLOCK_SIZE; ++i)
                                           {
                                               output[i] = triode.process_sample(input[i]);
                                           }
                                       }));
    }
    if (selected("DkCircuit", argc, argv))
    {
        print(measure_dk_circuit(), 0.12);
        print(measure_dk_potentiometer_update());
        print(measure_dk_potentiometer_rederivation());
    }
    if (selected("TubeScreamerBlock", argc, argv))
    {
        // No budget here: the 0.25 ms line belongs to the TubeScreamer node these two feed.
        print(measure_ts_clip_block());
        print(measure_ts_tone_block());
    }
    if (selected("PartitionedConvolution", argc, argv))
    {
        constexpr std::uint32_t impulse_length = 4096;
        std::vector<float> impulse(impulse_length);
        for (std::uint32_t i = 0; i < impulse_length; ++i)
        {
            impulse[i] = std::exp(-static_cast<float>(i) / 1000.0f) *
                         static_cast<float>(std::sin(0.1 * static_cast<double>(i)));
        }
        SushiDSP::PartitionedConvolution convolution(K_BLOCK_SIZE, impulse.data(), impulse_length);
        const std::vector<float> input = make_test_tone(K_BLOCK_SIZE);
        std::vector<float> output(K_BLOCK_SIZE, 0.0f);
        print(SushiDSP::bench::measure("PartitionedConvolution (4096 IR)", K_BLOCK_SIZE,
                                       K_SAMPLE_RATE,
                                       [&]
                                       {
                                           convolution.process(input.data(), output.data());
                                       }));
    }
    if (selected("Room", argc, argv))
    {
        print(measure_room_ears_steady_state(), 1.8);
        print(measure_room_ears_swap(), 2.8);
        print(measure_room_microphone_steady_state());
        print(measure_early_reflection_renderer());
        print(measure_modal_bank());
        print(measure_binaural_decoder());
        print(measure_room_beside_jcm800(), 2.0);
    }
    return 0;
}
