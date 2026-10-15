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
 * @file test_dk_block_node.cpp
 * @brief Tests that the DK block adapter drives a one-in, one-out RC low-pass through its float boundary.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuit_blocks/dk_block_node.hpp>
#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/math/dk/dk_circuit.hpp>
#include <SushiDSP/foundation/math/dk/dk_netlist.hpp>
#include <SushiDSP/foundation/math/dk/linear_port.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
} // namespace

TEST(Small_DkBlockNode, ReportsOneInputAndOneOutput)
{
    RcNode node;
    const SushiDSP::PortLayout ports = node.ports();
    EXPECT_EQ(ports.input_channels, 1u);
    EXPECT_EQ(ports.output_channels, 1u);
}

TEST(Small_DkBlockNode, PreparesAWellFormedBlock)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, 4800);
    EXPECT_TRUE(node.prepared());
}

TEST(Small_DkBlockNode, EmptyNetlistIsNotPrepared)
{
    RcNode node;
    prepare(node, 64);
    EXPECT_FALSE(node.prepared());
}

TEST(Small_DkBlockNode, LowPassSettlesOnADcInput)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, 4800);
    const std::vector<float> output = run(node, std::vector<float>(4800, 0.5f));
    EXPECT_NEAR(output.back(), 0.5f, 1e-4f);
}

TEST(Small_DkBlockNode, InputScaleCancelsAcrossTheBoundary)
{
    RcNode node;
    build_rc_low_pass(node);
    node.input_scale_volts = 10.0;
    prepare(node, 4800);
    const std::vector<float> output = run(node, std::vector<float>(4800, 0.5f));
    EXPECT_NEAR(output.back(), 0.5f, 1e-4f);
}

TEST(Small_DkBlockNode, LowPassRemovesANyquistInput)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, 4800);
    std::vector<float> input(4800);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
    const std::vector<float> output = run(node, input);
    float tail_peak = 0.0f;
    for (std::size_t i = output.size() - 480; i < output.size(); ++i)
    {
        tail_peak = std::max(tail_peak, std::fabs(output[i]));
    }
    EXPECT_LT(tail_peak, 0.01f);
}

TEST(Small_DkBlockNode, ResetReturnsTheBlockToRest)
{
    RcNode node;
    build_rc_low_pass(node);
    prepare(node, 4800);
    run(node, std::vector<float>(4800, 0.5f));
    node.reset();
    const std::vector<float> output = run(node, std::vector<float>(1, 0.0f));
    EXPECT_NEAR(output[0], 0.0f, 1e-6f);
}
