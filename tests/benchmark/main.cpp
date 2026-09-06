/**************************************************************************/
/* main.cpp                                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                        */
/**************************************************************************/

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Benchmark.hpp"
#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/DenormalGuard.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/dd3/Dd3Delay.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/circuits/stages/TriodeStage.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"
#include "SushiDSP/math/Resampler.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"
#include "SushiDSP/tubescreamer/TubeScreamerCircuits.hpp"

#include "circuits/JcmV1aNetlist.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr std::uint32_t kBlockSize = 256;

    /// @brief The 220 Hz tone the regression snapshots use, so the bench exercises the same path.
    std::vector<float> make_test_tone(std::uint32_t frames)
    {
        std::vector<float> tone(frames);
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            tone[i] = 0.25f * static_cast<float>(
                std::sin(2.0 * M_PI * 220.0 * static_cast<double>(i) / kSampleRate));
        }
        return tone;
    }

    /// @brief Times one node's process() over a block, the way a device callback calls it.
    template <typename Node, typename Configure>
    SushiDSP::bench::Result measure_node(const char* name, Configure&& configure)
    {
        Node node;
        node.prepare({ kSampleRate, kBlockSize, 1 });
        configure(node);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        const float* inputs[1] = { input.data() };
        float* outputs[1] = { output.data() };
        return SushiDSP::bench::measure(name, kBlockSize, kSampleRate, [&]
        {
            SushiDSP::ProcessContext context{ outputs, inputs, kBlockSize, 0.0 };
            node.process(context);
        });
    }

    // The netlist, its component constants and the triode glue live in
    // tests/circuits/JcmV1aNetlist.hpp, shared with the D6b stage test and the RT-safety traps.
    // build_jcm_v1a(netlist, true) appends the series RC that takes the block from the SPICE
    // stage's six reactive states to the seven the D6b budget is written against.
    using namespace SushiDSP::test_circuits;

    // The Tube Screamer's two blocks come from the product module, because the circuits are the
    // pedal: the bench prices exactly the description Task 7's node will run.
    using namespace SushiDSP::tube_screamer_circuits;

    constexpr std::uint32_t kOversampleFactor = 8;

    /// @brief The benchmarked shape: seven states, two nonlinear branches, two sources, one probe.
    using DkStageCircuit = SushiDSP::dk::DkCircuit<7, 2, 2, 1>;
    using DkStageNetlist = SushiDSP::dk::DkNetlist<7, 2, 2, 1>;

    /// @brief Runs the block on silence until its bias point has settled.
    /// @param circuit The prepared circuit; advanced in place.
    /// @param port The triode glue.
    /// @param run_rate Rate the circuit was prepared at, in hertz.
    void settle_dk_stage(DkStageCircuit& circuit, TriodeStagePort& port, double run_rate)
    {
        const std::array<double, 2> silence{ 0.0, kSupplyVolts };
        std::array<double, 1>       outputs{};
        const std::size_t           samples = static_cast<std::size_t>(0.2 * run_rate);
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
    template <typename Circuit>
    void print_solver_counters(const Circuit& circuit)
    {
        const std::uint64_t solves = circuit.total_samples();
        const std::uint64_t non_converged = circuit.non_converged_samples();
        std::printf("  solver: %llu solves, %llu non-converged (%.4f %%), %u iterations worst "
                    "case, %llu NaN fallbacks\n",
            static_cast<unsigned long long>(solves),
            static_cast<unsigned long long>(non_converged),
            solves == 0 ? 0.0 : 100.0 * static_cast<double>(non_converged)
                                    / static_cast<double>(solves),
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
        const double run_rate = kSampleRate * kOversampleFactor;

        DkStageNetlist netlist;
        build_jcm_v1a(netlist, true);

        DkStageCircuit circuit;
        TriodeStagePort   port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return { "DkCircuit 2-nonlin 7-state @8x", 0.0, 0.0 };
        }
        settle_dk_stage(circuit, port, run_rate);
        // Cleared after the settle and never again, so the counters below cover exactly the
        // solves measure() drives: every warm-up repetition and every timed one, 2048 per
        // repetition. total_samples() is the circuit's own count, so the reported rate cannot
        // drift from the harness's repetition constants.
        circuit.reset_statistics();

        const std::vector<float>  input = make_test_tone(kBlockSize);
        std::vector<float>        output(kBlockSize, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "DkCircuit 2-nonlin 7-state @8x", kBlockSize, kSampleRate,
            [&]
        {
            std::array<double, 1> outputs{};
            for (std::uint32_t frame = 0; frame < kBlockSize; ++frame)
            {
                const double drive = static_cast<double>(input[frame])
                    * static_cast<double>(SushiDSP::kFullScaleVolts);
                const std::array<double, 2> inputs{ drive, kSupplyVolts };
                for (std::uint32_t step = 0; step < kOversampleFactor; ++step)
                {
                    circuit.process_sample(port, inputs, outputs);
                }
                output[frame] = static_cast<float>(outputs[0]);
            }
        });

        // The interelectrode capacitances make the buffer wrap cost no solve, so this count is
        // zero; test_dk_triode_stage.cpp measures the honest figure for a real stream.
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
        DkStageNetlist    netlist;
        const std::size_t pot_index = build_jcm_v1a(netlist, true);

        DkStageCircuit circuit;
        TriodeStagePort   port;
        if (!circuit.prepare(netlist, kSampleRate))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return { "DkCircuit pot update (1x)", 0.0, 0.0 };
        }
        settle_dk_stage(circuit, port, kSampleRate);

        const std::vector<float>  input = make_test_tone(kBlockSize);
        std::vector<float>        output(kBlockSize, 0.0f);
        double                    position = 0.5;
        return SushiDSP::bench::measure("DkCircuit pot update (1x)", kBlockSize, kSampleRate, [&]
        {
            position = position > 0.5 ? 0.4 : 0.6;
            circuit.set_potentiometer(pot_index, position);
            circuit.update_matrices_if_dirty();

            std::array<double, 1> outputs{};
            for (std::uint32_t frame = 0; frame < kBlockSize; ++frame)
            {
                const double drive = static_cast<double>(input[frame])
                    * static_cast<double>(SushiDSP::kFullScaleVolts);
                const std::array<double, 2> inputs{ drive, kSupplyVolts };
                circuit.process_sample(port, inputs, outputs);
                output[frame] = static_cast<float>(outputs[0]);
            }
        });
    }

    /// @brief Times the potentiometer re-derivation on its own, with no solving around it.
    ///
    /// Plan Ruling 3 lets a pot move re-derive the DK matrices on the audio thread at block
    /// cadence, so the cost of one re-derivation is the number the ruling rests on. The case
    /// above cannot supply it - a re-derivation is under one percent of the block it sits in, so
    /// subtracting two block timings would report scheduler noise. This one calls
    /// set_potentiometer + update_matrices_if_dirty 256 times and nothing else, so the harness's
    /// per-sample column reads directly as nanoseconds per re-derivation.
    /// @return The measured Result, whose ns/sample column is ns per re-derivation.
    SushiDSP::bench::Result measure_dk_potentiometer_rederivation()
    {
        DkStageNetlist    netlist;
        const std::size_t pot_index = build_jcm_v1a(netlist, true);

        DkStageCircuit  circuit;
        TriodeStagePort port;
        if (!circuit.prepare(netlist, kSampleRate))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return { "DkCircuit pot re-derivation", 0.0, 0.0 };
        }
        settle_dk_stage(circuit, port, kSampleRate);

        double position = 0.5;
        return SushiDSP::bench::measure("DkCircuit pot re-derivation", kBlockSize, kSampleRate,
            [&]
        {
            for (std::uint32_t index = 0; index < kBlockSize; ++index)
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
        const double run_rate = kSampleRate * kOversampleFactor;

        ClipNetlist       netlist;
        const std::size_t pot = build_clip_block(netlist);

        ClipCircuit   circuit;
        DiodePairPort port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("ts_clip prepare failed; case skipped\n");
            return { "TubeScreamerBlock clip @8x", 0.0, 0.0 };
        }
        circuit.set_potentiometer(pot, drive_to_position(0.7));
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        const std::vector<float>      input = make_test_tone(kBlockSize);
        std::vector<float>            output(kBlockSize, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "TubeScreamerBlock clip @8x", kBlockSize, kSampleRate,
            [&]
        {
            std::array<double, 1> outputs{};
            for (std::uint32_t frame = 0; frame < kBlockSize; ++frame)
            {
                const std::array<double, 1> inputs{ static_cast<double>(input[frame])
                    * static_cast<double>(SushiDSP::kFullScaleVolts) };
                for (std::uint32_t step = 0; step < kOversampleFactor; ++step)
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
        const double run_rate = kSampleRate * kOversampleFactor;

        ToneNetlist              netlist;
        const TonePotentiometers pots = build_tone_block(netlist);

        ToneCircuit circuit;
        LinearPort  port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("ts_tone prepare failed; case skipped\n");
            return { "TubeScreamerBlock tone @8x", 0.0, 0.0 };
        }
        circuit.set_potentiometer(pots.tone, tone_to_position(0.5));
        circuit.set_potentiometer(pots.level, kLevelPotPosition);
        circuit.update_matrices_if_dirty();
        circuit.reset_statistics();

        const std::vector<float>      input = make_test_tone(kBlockSize);
        std::vector<float>            output(kBlockSize, 0.0f);
        const SushiDSP::bench::Result result = SushiDSP::bench::measure(
            "TubeScreamerBlock tone @8x", kBlockSize, kSampleRate,
            [&]
        {
            std::array<double, 1> outputs{};
            for (std::uint32_t frame = 0; frame < kBlockSize; ++frame)
            {
                const std::array<double, 1> inputs{ static_cast<double>(input[frame])
                    * static_cast<double>(SushiDSP::kFullScaleVolts) };
                for (std::uint32_t step = 0; step < kOversampleFactor; ++step)
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
    ///
    /// 48 kHz down to 20 kHz and back is the pair's worst case: the decimating stage's half span
    /// is 34 / (20000 / 48000) = 81.6 input samples, so interpolate() runs two loops of 82 and
    /// touches 164 taps per output. Both resamplers are prepared outside the timed body, since
    /// prepare() is where every buffer is allocated.
    ///
    /// The case prices the resampler pair in isolation at that clock; it does not decompose the
    /// DD-3's own number. The node pushes 256 single frames per block where this pushes one block
    /// of 256, and its clock follows D.TIME instead of sitting at 20 kHz, so the kernel span
    /// differs as well. That is why the 0.10 ms it prints against is advisory.
    /// @return The measured Result, costed per output sample at the base rate.
    SushiDSP::bench::Result measure_resampler_round_trip()
    {
        constexpr double slow_clock_hz = 20000.0;

        SushiDSP::Resampler down;
        SushiDSP::Resampler up;
        down.prepare(kSampleRate, slow_clock_hz, kBlockSize, slow_clock_hz / kSampleRate);
        const std::uint32_t clock_capacity = down.maximum_output_frames(kBlockSize) + 8u;
        up.prepare(slow_clock_hz, kSampleRate, clock_capacity, kSampleRate / 82500.0);
        const std::uint32_t base_capacity = up.maximum_output_frames(clock_capacity) + 8u;

        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float>       at_clock_rate(clock_capacity, 0.0f);
        std::vector<float>       back_at_base_rate(base_capacity, 0.0f);

        return SushiDSP::bench::measure("Resampler 48k-20k-48k round trip", kBlockSize,
            kSampleRate, [&]
        {
            const std::uint32_t produced = down.process(input.data(), kBlockSize,
                at_clock_rate.data(), clock_capacity);
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
}

int main(int argc, char** argv)
{
    using SushiDSP::bench::print;
    // Same FPU mode the device callbacks run under, so the numbers match the real thread.
    SushiDSP::DenormalGuard denormal_guard;
    std::printf("SushiDSP benchmark  (%g Hz, %u-frame blocks, best of 200)\n\n", kSampleRate, kBlockSize);

    if (selected("Jcm800", argc, argv))
    {
        print(measure_node<SushiDSP::Jcm800>("Jcm800 (no cabinet)", [](SushiDSP::Jcm800& node)
        {
            node.set_preamp(0.6f);
            node.set_master(0.0f);
            node.set_bass(0.6f);
            node.set_middle(0.4f);
            node.set_treble(0.7f);
            node.set_presence(0.5f);
        }), 1.5);
    }
    if (selected("TubeScreamer", argc, argv))
    {
        print(measure_node<SushiDSP::TubeScreamer>("TubeScreamer", [](SushiDSP::TubeScreamer& node)
        {
            node.set_drive(0.7f);
            node.set_tone(0.5f);
            node.set_level(0.8f);
        }), 0.25);
    }
    if (selected("Dd3", argc, argv))
    {
        print(measure_node<SushiDSP::Dd3Delay>("DD-3 Delay", [](SushiDSP::Dd3Delay& node)
        {
            node.set_mode(SushiDSP::dd3::Dd3Mode::Middle);
            node.set_time(0.4f);
            node.set_feedback(0.45f);
            node.set_level(0.5f);
        }), 0.15);
    }
    if (selected("Resampler", argc, argv))
    {
        print(measure_resampler_round_trip(), 0.10);
    }
    if (selected("Oversampler8x", argc, argv))
    {
        SushiDSP::Oversampler8x oversampler(kBlockSize);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("Oversampler8x round trip", kBlockSize, kSampleRate, [&]
        {
            const float* upsampled = oversampler.upsample(input.data(), kBlockSize);
            oversampler.downsample(upsampled, kBlockSize * 8, output.data());
        }));
    }
    if (selected("TriodeStage", argc, argv))
    {
        // The 12AX7 values Jcm800 uses for V1a.
        SushiDSP::TriodeStage triode(10000.0f, 1000000.0f, -1.5f, 60.0f, 1e-9f, 0.02585f);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("TriodeStage (base rate)", kBlockSize, kSampleRate, [&]
        {
            for (std::uint32_t i = 0; i < kBlockSize; ++i)
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
            impulse[i] = std::exp(-static_cast<float>(i) / 1000.0f)
                * static_cast<float>(std::sin(0.1 * static_cast<double>(i)));
        }
        SushiDSP::PartitionedConvolution convolution(kBlockSize, impulse.data(), impulse_length);
        const std::vector<float> input = make_test_tone(kBlockSize);
        std::vector<float> output(kBlockSize, 0.0f);
        print(SushiDSP::bench::measure("PartitionedConvolution (4096 IR)", kBlockSize, kSampleRate, [&]
        {
            convolution.process(input.data(), output.data());
        }));
    }
    return 0;
}
