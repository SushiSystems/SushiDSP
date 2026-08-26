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
#include <cstdio>
#include <cstring>
#include <vector>

#include "Benchmark.hpp"
#include "SushiDSP/circuits/models/Triode12AX7.hpp"
#include "SushiDSP/core/Calibration.hpp"
#include "SushiDSP/core/DenormalGuard.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/jcm800/Jcm800.hpp"
#include "SushiDSP/jcm800/TriodeStage.hpp"
#include "SushiDSP/math/Oversampler.hpp"
#include "SushiDSP/math/PartitionedConvolution.hpp"
#include "SushiDSP/math/dk/DkCircuit.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"
#include "SushiDSP/tubescreamer/TubeScreamer.hpp"

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

    /// @brief Node numbering of the benchmarked DK block.
    ///
    /// Nodes 1-7 are the JCM800 2203 V1a stage, node for node with
    /// docs/reference/spice/jcm_v1a; nodes 8 and 9 carry the extra series RC that takes the
    /// block from three states to the four the D6b budget is written against.
    enum DkNodeIndex : std::size_t
    {
        kDkNodeSource = 1,
        kDkNodeGrid = 2,
        kDkNodePlate = 3,
        kDkNodeCathode = 4,
        kDkNodePotTop = 5,
        kDkNodeOut = 6,
        kDkNodeSupply = 7,
        kDkNodeTailMiddle = 8,
        kDkNodeTail = 9
    };

    constexpr double        kDkSupplyVolts = 360.0;
    constexpr std::uint32_t kOversampleFactor = 8;

    /// @brief The benchmarked shape: four states, two nonlinear branches, two sources, one probe.
    using DkStageCircuit = SushiDSP::dk::DkCircuit<4, 2, 2, 1>;
    using DkStageNetlist = SushiDSP::dk::DkNetlist<4, 2, 2, 1>;

    /// @brief Hands DkCircuit the 12AX7's two branch currents and their Jacobian.
    struct DkTriodePort
    {
        SushiDSP::Triode12AX7 triode{};

        /// @brief Evaluates both branch currents at one port voltage.
        /// @param v Port voltages: v[0] = Vgk, v[1] = Vpk.
        /// @param i Receives the branch currents in amperes.
        void currents(const std::array<double, 2>& v, std::array<double, 2>& i) const
        {
            triode.current(v, i);
        }

        /// @brief Evaluates the branch-current Jacobian at one port voltage.
        /// @param v Port voltages: v[0] = Vgk, v[1] = Vpk.
        /// @param j Receives the 2x2 Jacobian in siemens.
        void jacobian(const std::array<double, 2>& v, std::array<std::array<double, 2>, 2>& j) const
        {
            triode.jacobian(v, j);
        }
    };

    /// @brief Builds the benchmarked netlist.
    /// @param netlist Receives the description; any previous contents are overwritten.
    /// @return The preamp potentiometer's index, for DkCircuit::set_potentiometer.
    std::size_t build_dk_stage(DkStageNetlist& netlist)
    {
        netlist = DkStageNetlist{};
        netlist.node_count = 9;

        netlist.resistor(kDkNodeSource, 0, 1.0e6);
        netlist.resistor(kDkNodeSource, kDkNodeGrid, 68.0e3);
        netlist.resistor(kDkNodeSupply, kDkNodePlate, 100.0e3);
        netlist.resistor(kDkNodeCathode, 0, 2.7e3);

        netlist.capacitor(kDkNodeCathode, 0, 0.68e-6);
        netlist.capacitor(kDkNodePlate, kDkNodePotTop, 0.022e-6);
        netlist.capacitor(kDkNodePotTop, kDkNodeOut, 470.0e-12);

        netlist.resistor(kDkNodeOut, kDkNodeTailMiddle, 470.0e3);
        netlist.capacitor(kDkNodeTailMiddle, kDkNodeTail, 0.022e-6);
        netlist.resistor(kDkNodeTail, 0, 1.0e6);

        netlist.nonlinear(kDkNodeGrid, kDkNodeCathode);
        netlist.nonlinear(kDkNodePlate, kDkNodeCathode);

        netlist.voltage_source(kDkNodeSource, 0);
        netlist.voltage_source(kDkNodeSupply, 0);

        netlist.output(kDkNodeTail, 0);

        return netlist.potentiometer(kDkNodePotTop, kDkNodeOut, 0, 1.0e6);
    }

    /// @brief Runs the block on silence until its bias point has settled.
    /// @param circuit The prepared circuit; advanced in place.
    /// @param port The triode glue.
    /// @param run_rate Rate the circuit was prepared at, in hertz.
    void settle_dk_stage(DkStageCircuit& circuit, DkTriodePort& port, double run_rate)
    {
        const std::array<double, 2> silence{ 0.0, kDkSupplyVolts };
        std::array<double, 1>       outputs{};
        const std::size_t           samples = static_cast<std::size_t>(0.2 * run_rate);
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
    /// @return The measured Result, costed per output sample at the base rate.
    SushiDSP::bench::Result measure_dk_circuit()
    {
        const double run_rate = kSampleRate * kOversampleFactor;

        DkStageNetlist netlist;
        build_dk_stage(netlist);

        DkStageCircuit circuit;
        DkTriodePort   port;
        if (!circuit.prepare(netlist, run_rate))
        {
            std::printf("DkCircuit prepare failed; case skipped\n");
            return { "DkCircuit 2-nonlin 4-state @8x", 0.0, 0.0 };
        }
        settle_dk_stage(circuit, port, run_rate);

        const std::vector<float>  input = make_test_tone(kBlockSize);
        std::vector<float>        output(kBlockSize, 0.0f);
        return SushiDSP::bench::measure("DkCircuit 2-nonlin 4-state @8x", kBlockSize, kSampleRate,
            [&]
        {
            std::array<double, 1> outputs{};
            for (std::uint32_t frame = 0; frame < kBlockSize; ++frame)
            {
                const double drive = static_cast<double>(input[frame])
                    * static_cast<double>(SushiDSP::kFullScaleVolts);
                const std::array<double, 2> inputs{ drive, kDkSupplyVolts };
                for (std::uint32_t step = 0; step < kOversampleFactor; ++step)
                {
                    circuit.process_sample(port, inputs, outputs);
                }
                output[frame] = static_cast<float>(outputs[0]);
            }
        });
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
        const std::size_t pot_index = build_dk_stage(netlist);

        DkStageCircuit circuit;
        DkTriodePort   port;
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
                const std::array<double, 2> inputs{ drive, kDkSupplyVolts };
                circuit.process_sample(port, inputs, outputs);
                output[frame] = static_cast<float>(outputs[0]);
            }
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
