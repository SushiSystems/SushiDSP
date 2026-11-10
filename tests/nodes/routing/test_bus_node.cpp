/**************************************************************************/
/* test_bus_node.cpp                                                      */
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
 * @file test_bus_node.cpp
 * @brief Tests the mixer bus: mute and solo over its stereo inputs, and its RigGraph routing.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/nodes/routing/bus_node.hpp>
#include <SushiDSP/nodes/routing/channel_strip_node.hpp>

#include "support/tolerance.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    constexpr std::uint32_t K_BLOCK = 256;
    constexpr std::uint32_t K_SLOTS = SushiDSP::BusNode::K_MAX_INPUTS;
    constexpr double K_RATE = 48000.0;

    /// @brief Holds the gate smoothing the header documents, in milliseconds.
    constexpr double K_DOCUMENTED_GATE_MS = 5.0;

    /// @brief Holds the gate's time constant in frames: 5 ms at 48 kHz is 240.
    constexpr double K_GATE_FRAMES = K_DOCUMENTED_GATE_MS * K_RATE / 1000.0;

    /// @brief Holds the blocks run_settled runs: ten blocks of 256 frames is 53 ms.
    constexpr int K_SETTLE_BLOCKS = 10;

    /// @brief Holds the frames a closing gate moves in run_settled.
    constexpr double K_SETTLE_FRAMES = static_cast<double>(K_SETTLE_BLOCKS) * K_BLOCK;

    /// @brief Counts the units of u that bound a mixed sample relative to the sum of its inputs
    /// at full gate: four roundings in the products and the sums of the two stereo inputs. The
    /// closing gate's own float error is at most K_SETTLE_FRAMES u of a residual of 2.3e-5, or
    /// 0.06 u of the input, which sits inside the fifth unit.
    constexpr double K_MIX_UNITS = 5.0;

    /// @brief Bounds the absolute error of one hard-panned strip's channel that is read through the
    /// bus: 5 u for the pan angle (see test_pan_node.cpp), and one u for the bus sum.
    constexpr double K_STRIP_ROUTE_BOUND = 6.0 * TestSupport::unit_roundoff<float>();

    /// @brief A stereo block of one input: left and right levels, held constant.
    struct StereoBlock
    {
        std::vector<float> left;
        std::vector<float> right;

        StereoBlock(float l, float r) : left(K_BLOCK, l), right(K_BLOCK, r) {}
    };

    /// @brief Returns the fraction of a gate that is left after run_settled: exp(-N / n).
    double gate_residual()
    {
        return std::exp(-K_SETTLE_FRAMES / K_GATE_FRAMES);
    }

    /// @brief Runs one block through the bus with the given slots connected; null is unconnected.
    void run_bus(SushiDSP::BusNode& bus, const std::vector<const StereoBlock*>& slots,
                 std::vector<float>& out_left, std::vector<float>& out_right)
    {
        const float* inputs[2 * K_SLOTS] = {};
        for (std::uint32_t slot = 0; slot < K_SLOTS; ++slot)
        {
            if (slot < slots.size() && slots[slot] != nullptr)
            {
                inputs[2 * slot] = slots[slot]->left.data();
                inputs[2 * slot + 1] = slots[slot]->right.data();
            }
        }
        float* outputs[2] = {out_left.data(), out_right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
        bus.process(context);
    }

    /// @brief Prepares a bus at 48 kHz.
    void prepare(SushiDSP::BusNode& bus)
    {
        bus.prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    }

    /// @brief Runs K_SETTLE_BLOCKS blocks, so an open gate closes by K_SETTLE_FRAMES frames.
    void run_settled(SushiDSP::BusNode& bus, const std::vector<const StereoBlock*>& slots,
                     std::vector<float>& out_left, std::vector<float>& out_right)
    {
        for (int block = 0; block < K_SETTLE_BLOCKS; ++block)
            run_bus(bus, slots, out_left, out_right);
    }

    /// @brief Expects @p computed to be @p reference within K_MIX_UNITS of u times @p magnitude.
    void expect_mix(float computed, double reference, double magnitude)
    {
        const double bound = K_MIX_UNITS * TestSupport::unit_roundoff<float>() * magnitude;
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(computed, reference, bound)));
    }
} // namespace

TEST(Small_BusNode, BusSumsTwoConnectedStereoInputs)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_bus(bus, {&a, &b}, left, right);

    EXPECT_FLOAT_EQ(left[K_BLOCK - 1], 0.75f);
    EXPECT_FLOAT_EQ(right[K_BLOCK - 1], 0.75f);
}

TEST(Small_BusNode, BusMuteSilencesItsInput)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_bus(bus, {&a, &b}, left, right);
    bus.set_parameter_for_control_thread("mute_0", 1.0f);

    run_settled(bus, {&a, &b}, left, right);

    // Input 0 is gated shut and has decayed to its residual; input 1 is left whole.
    expect_mix(left[K_BLOCK - 1], 0.5 + 0.25 * gate_residual(), 0.75);
    expect_mix(right[K_BLOCK - 1], 0.25 + 0.5 * gate_residual(), 0.75);
}

TEST(Small_BusNode, BusSoloPassesOnlyTheSoloedInput)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_bus(bus, {&a, &b}, left, right);
    bus.set_parameter_for_control_thread("solo_1", 1.0f);

    run_settled(bus, {&a, &b}, left, right);

    expect_mix(left[K_BLOCK - 1], 0.5 + 0.25 * gate_residual(), 0.75);
    expect_mix(right[K_BLOCK - 1], 0.25 + 0.5 * gate_residual(), 0.75);
}

TEST(Small_BusNode, BusMuteWinsOverSolo)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_bus(bus, {&a, &b}, left, right);
    bus.set_parameter_for_control_thread("solo_0", 1.0f);
    bus.set_parameter_for_control_thread("mute_0", 1.0f);

    run_settled(bus, {&a, &b}, left, right);

    // Input 0 is soloed but muted and input 1 is unsoloed, so both close to their residual.
    expect_mix(left[K_BLOCK - 1], 0.75 * gate_residual(), 0.75);
    expect_mix(right[K_BLOCK - 1], 0.75 * gate_residual(), 0.75);
}

TEST(Small_BusNode, BusSoloOnAnUnconnectedSlotDoesNotSilenceTheRest)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    bus.set_parameter_for_control_thread("solo_5", 1.0f);
    const StereoBlock a(0.25f, 0.5f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_bus(bus, {&a}, left, right);

    EXPECT_FLOAT_EQ(left[K_BLOCK - 1], 0.25f);
    EXPECT_FLOAT_EQ(right[K_BLOCK - 1], 0.5f);
}

TEST(Small_BusNode, BusGateMovesOverFiveMillisecondsNotInAStep)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    const StereoBlock a(1.0f, 1.0f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);
    run_bus(bus, {&a}, left, right);

    bus.set_parameter_for_control_thread("mute_0", 1.0f);
    run_bus(bus, {&a}, left, right);

    // The first frame after the toggle is still nearly open; the gate has fallen by the last.
    EXPECT_GT(left[0], 0.99f);
    EXPECT_LT(left[K_BLOCK - 1], 0.5f);
}

TEST(Small_BusNode, BusStartsAMutedInputSilentOnItsFirstBlock)
{
    // A bus built with an input already muted never lets it through while a gate closes.
    SushiDSP::BusNode bus;
    prepare(bus);
    bus.set_parameter_for_control_thread("mute_0", 1.0f);
    const StereoBlock a(1.0f, 1.0f);
    const StereoBlock b(0.25f, 0.5f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_bus(bus, {&a, &b}, left, right);

    EXPECT_FLOAT_EQ(left[0], 0.25f);
    EXPECT_FLOAT_EQ(right[0], 0.5f);
    EXPECT_FLOAT_EQ(left[K_BLOCK - 1], 0.25f);
}

TEST(Small_BusNode, BusUnknownParameterIdChangesNoMuteOrSolo)
{
    SushiDSP::BusNode bus;
    bus.set_parameter_for_control_thread("solo_3", 1.0f);

    bus.set_parameter_for_control_thread("no_such_parameter", 1.0f);

    EXPECT_FLOAT_EQ(bus.get_parameter("solo_3"), 1.0f);
    EXPECT_FLOAT_EQ(bus.get_parameter("solo_2"), 0.0f);
    EXPECT_FLOAT_EQ(bus.get_parameter("mute_3"), 0.0f);
    EXPECT_FLOAT_EQ(bus.get_parameter("no_such_parameter"), 0.0f);
}

TEST(Small_BusNode, BusRoutesEachStripToItsOwnSlotThroughRigGraph)
{
    // Strip A is hard left and strip B hard right, so the bus is left from A and right from B.
    SushiDSP::RigGraph graph(K_BLOCK, 8);
    auto strip_a = std::make_unique<SushiDSP::ChannelStripNode>();
    strip_a->set_parameter_for_control_thread("pan", -1.0f);
    auto strip_b = std::make_unique<SushiDSP::ChannelStripNode>();
    strip_b->set_parameter_for_control_thread("pan", 1.0f);
    const auto handle_a = graph.add_node(std::move(strip_a));
    const auto handle_b = graph.add_node(std::move(strip_b));
    const auto bus = graph.add_node(std::make_unique<SushiDSP::BusNode>());
    graph.connect(handle_a, bus);
    graph.connect(handle_b, bus);
    graph.compile(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
    std::vector<float> live(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK, 0.0f), right(K_BLOCK, 0.0f);
    const float* live_inputs[1] = {live.data()};
    float* outputs[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{outputs, live_inputs, K_BLOCK, 0.0};

    graph.process(context);

    // Strip A contributes 1.0 to the left and 0.0 to the right; strip B the mirror image.
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(left[K_BLOCK - 1], 1.0, K_STRIP_ROUTE_BOUND)));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_error(right[K_BLOCK - 1], 1.0, K_STRIP_ROUTE_BOUND)));
}
