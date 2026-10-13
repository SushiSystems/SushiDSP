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

/** @file test_bus_node.cpp
 *  @brief Tests the mixer bus: mute and solo over its stereo inputs, and its routing through RigGraph.
 *  @author Mustafa Garip
 */

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/core/audio_node.hpp"
#include "SushiDSP/core/rig_graph.hpp"
#include "SushiDSP/routing/bus_node.hpp"
#include "SushiDSP/routing/channel_strip_node.hpp"

namespace
{
    constexpr std::uint32_t K_BLOCK = 256;
    constexpr std::uint32_t K_SLOTS = SushiDSP::BusNode::K_MAX_INPUTS;

    /// @brief A stereo block of one input: left and right levels, held constant.
    struct StereoBlock
    {
        std::vector<float> left;
        std::vector<float> right;

        StereoBlock(float l, float r) : left(K_BLOCK, l), right(K_BLOCK, r) {}
    };

    /// @brief Runs one block through the bus with the given slots connected; a null slot is unconnected.
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
        bus.prepare(SushiDSP::PrepareInfo{48000.0, K_BLOCK, 2});
    }

    /// @brief Runs blocks until a 5 ms gate has settled: ten blocks of 256 frames is 53 ms, about
    /// ten time constants.
    void run_settled(SushiDSP::BusNode& bus, const std::vector<const StereoBlock*>& slots,
                     std::vector<float>& out_left, std::vector<float>& out_right)
    {
        for (int block = 0; block < 10; ++block)
        {
            run_bus(bus, slots, out_left, out_right);
        }
    }
} // namespace

TEST(Unit_Dsp, BusSumsTwoConnectedStereoInputs)
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

TEST(Unit_Dsp, BusMuteSilencesItsInput)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    bus.set_parameter_for_control_thread("mute_0", 1.0f);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_settled(bus, {&a, &b}, left, right);

    // Input 0 is gated shut; only input 1 is left.
    EXPECT_NEAR(left[K_BLOCK - 1], 0.5f, 1e-3f);
    EXPECT_NEAR(right[K_BLOCK - 1], 0.25f, 1e-3f);
}

TEST(Unit_Dsp, BusSoloPassesOnlyTheSoloedInput)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    bus.set_parameter_for_control_thread("solo_1", 1.0f);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_settled(bus, {&a, &b}, left, right);

    EXPECT_NEAR(left[K_BLOCK - 1], 0.5f, 1e-3f);
    EXPECT_NEAR(right[K_BLOCK - 1], 0.25f, 1e-3f);
}

TEST(Unit_Dsp, BusMuteWinsOverSolo)
{
    SushiDSP::BusNode bus;
    prepare(bus);
    bus.set_parameter_for_control_thread("solo_0", 1.0f);
    bus.set_parameter_for_control_thread("mute_0", 1.0f);
    const StereoBlock a(0.25f, 0.5f);
    const StereoBlock b(0.5f, 0.25f);
    std::vector<float> left(K_BLOCK), right(K_BLOCK);

    run_settled(bus, {&a, &b}, left, right);

    // Input 0 is soloed but muted, so it stays silent and input 1, unsoloed, is silent too.
    EXPECT_NEAR(left[K_BLOCK - 1], 0.0f, 1e-3f);
    EXPECT_NEAR(right[K_BLOCK - 1], 0.0f, 1e-3f);
}

TEST(Unit_Dsp, BusSoloOnAnUnconnectedSlotDoesNotSilenceTheRest)
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

TEST(Unit_Dsp, BusGateMovesOverFiveMillisecondsNotInAStep)
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

TEST(Unit_Dsp, BusStartsAMutedInputSilentOnItsFirstBlock)
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

TEST(Unit_Dsp, BusParametersReadBackAndRejectUnknownNames)
{
    SushiDSP::BusNode bus;
    bus.set_parameter_for_control_thread("solo_3", 1.0f);

    EXPECT_FLOAT_EQ(bus.get_parameter("solo_3"), 1.0f);
    EXPECT_FLOAT_EQ(bus.get_parameter("solo_2"), 0.0f);
    EXPECT_FLOAT_EQ(bus.get_parameter("no_such_parameter"), 0.0f);
}

TEST(Unit_Dsp, BusRoutesEachStripToItsOwnSlotThroughRigGraph)
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
    graph.compile(SushiDSP::PrepareInfo{48000.0, K_BLOCK, 2});

    std::vector<float> live(K_BLOCK, 1.0f);
    std::vector<float> left(K_BLOCK, 0.0f), right(K_BLOCK, 0.0f);
    const float* live_inputs[1] = {live.data()};
    float* outputs[2] = {left.data(), right.data()};
    SushiDSP::ProcessContext context{outputs, live_inputs, K_BLOCK, 0.0};
    graph.process(context);

    // Strip A contributes 1.0 to the left and 0.0 to the right; strip B the mirror image.
    EXPECT_NEAR(left[K_BLOCK - 1], 1.0f, 1e-3f);
    EXPECT_NEAR(right[K_BLOCK - 1], 1.0f, 1e-3f);
}
