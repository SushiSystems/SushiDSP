/**************************************************************************/
/* test_dk_block_node.cpp                                                 */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_dk_block_node.cpp
 * @brief Tests that the DK block adapter drives a one-in, one-out RC low-pass through its float boundary.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/circuit_blocks/dk_block_node.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    /// @brief One capacitor, no nonlinear branch, one input source and one output probe.
    using RcNetlist = SushiDSP::dk::DkNetlist<1, 0, 1, 1>;

    /// @brief The circuit that solves RcNetlist.
    using RcCircuit = SushiDSP::dk::DkCircuit<1, 0, 1, 1>;

    /// @brief The adapter under test, wrapped around the RC low-pass.
    using RcNode = SushiDSP::test_circuits::DkBlockNode<RcNetlist, RcCircuit, SushiDSP::dk::LinearPort>;

    /// @brief The run rate every case prepares the node at.
    constexpr double K_SAMPLE_RATE = 48000.0;

    /// @brief Builds a 1 kOhm, 100 nF low-pass: source at node 1, output across the capacitor at node 2.
    /// @param node The adapter whose netlist is filled.
    void build_rc_low_pass(RcNode& node)
    {
        node.netlist = RcNetlist{};
        node.netlist.node_count = 2;
        node.netlist.voltage_source(1, 0);
        node.netlist.resistor(1, 2, 1.0e3);
        node.netlist.capacitor(2, 0, 100.0e-9);
        node.netlist.output(2, 0);
    }

    /// @brief Runs a stimulus through the node in one block and returns the output.
    /// @param node A prepared node.
    /// @param input The stimulus; its length is the block size.
    std::vector<float> run(RcNode& node, const std::vector<float>& input)
    {
        std::vector<float> output(input.size(), 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[1] = {output.data()};
        SushiDSP::ProcessContext context{outputs, inputs, static_cast<std::uint32_t>(input.size()), 0.0};
        node.process(context);
        return output;
    }

    /// @brief Prepares a node for a block of the given length.
    /// @param node The node to prepare.
    /// @param frames The largest block it will see.
    void prepare(RcNode& node, std::size_t frames)
    {
        node.prepare(SushiDSP::PrepareInfo{K_SAMPLE_RATE, static_cast<std::uint32_t>(frames), 1u});
    }

    /// @brief Holds the block length every settling and tail case runs at: 1000 time constants of the low-pass.
    constexpr std::size_t K_FRAMES = 4800;

    /// @brief Holds the steady input level of the settling cases.
    constexpr float K_DC_LEVEL = 0.5f;

    /// @brief Holds the length of the tail the Nyquist case inspects: the last 480 samples.
    constexpr std::size_t K_TAIL_FRAMES = 480;

    /// @brief Builds a stimulus that alternates between +1 and -1 on every sample: a tone at Nyquist.
    /// @param frames The stimulus length.
    std::vector<float> nyquist_tone(std::size_t frames)
    {
        std::vector<float> tone(frames);
        for (std::size_t i = 0; i < frames; ++i)
        {
            tone[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        }
        return tone;
    }

    /// @brief Returns the largest magnitude among the last @p tail samples.
    /// @param signal The signal to inspect; at least @p tail long.
    /// @param tail How many samples at the end to inspect.
    float tail_peak(const std::vector<float>& signal, std::size_t tail)
    {
        float peak = 0.0f;
        for (std::size_t i = signal.size() - tail; i < signal.size(); ++i)
        {
            peak = std::max(peak, std::fabs(signal[i]));
        }
        return peak;
    }

    /// @brief Describes one node setup and whether prepare() accepts it.
    struct PrepareCase
    {
            const char* name;
            bool builds_netlist;
            bool expected_prepared;
    };

    const PrepareCase kPrepareCases[] = {
        {"WellFormedBlock", true, true},
        {"EmptyNetlist", false, false},
    };
} // namespace

class Small_DkBlockNodePrepares : public ::testing::TestWithParam<PrepareCase>
{
};

TEST_P(Small_DkBlockNodePrepares, ReportsWhetherTheNetlistIsUsable)
{
    RcNode node;
    if (GetParam().builds_netlist)
    {
        build_rc_low_pass(node);
    }
    prepare(node, K_FRAMES);
    EXPECT_EQ(node.prepared(), GetParam().expected_prepared);
}

INSTANTIATE_TEST_SUITE_P(Netlists, Small_DkBlockNodePrepares, ::testing::ValuesIn(kPrepareCases),
                         [](const ::testing::TestParamInfo<PrepareCase>& info) { return std::string(info.param.name); });

// The settled value is 0.5 up to the division by the input scale and the cast to float: two rounded operations.
TEST(Small_DkBlockNode, LowPassSettlesOnADcInput)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, K_FRAMES);
    const std::vector<float> output = run(node, std::vector<float>(K_FRAMES, K_DC_LEVEL));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(output.back(), K_DC_LEVEL, 2)));
}

// A scale of 10 multiplies the drive and divides the output; both are exact in double for 0.5, so the same two roundings remain.
TEST(Small_DkBlockNode, InputScaleCancelsAcrossTheBoundary)
{
    RcNode node;
    build_rc_low_pass(node);
    node.input_scale_volts = 10.0;
    prepare(node, K_FRAMES);
    const std::vector<float> output = run(node, std::vector<float>(K_FRAMES, K_DC_LEVEL));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(output.back(), K_DC_LEVEL, 2)));
}

// The trapezoidal rule puts a zero at z = -1 and the transient decays by 0.81 per sample (a = T / (2RC) = 0.104), so only rounding stays: one double addition per sample.
TEST(Small_DkBlockNode, LowPassRemovesANyquistInput)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, K_FRAMES);
    const std::vector<float> output = run(node, nyquist_tone(K_FRAMES));
    const double rounding_bound = static_cast<double>(K_FRAMES) * TestSupport::unit_roundoff<double>();
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(tail_peak(output, K_TAIL_FRAMES), 0.0, rounding_bound)));
}

// A zero state driven by a zero input gives a zero output exactly.
TEST(Small_DkBlockNode, ResetReturnsTheBlockToRest)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, K_FRAMES);
    run(node, std::vector<float>(K_FRAMES, K_DC_LEVEL));
    node.reset();
    const std::vector<float> output = run(node, std::vector<float>(1, 0.0f));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(output[0], 0.0, 0.0)));
}
