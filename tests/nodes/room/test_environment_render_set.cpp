/**************************************************************************/
/* test_environment_render_set.cpp                                        */
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
 * @file test_environment_render_set.cpp
 * @brief Tests the room node as an environment: several cabinets, the listener and microphones on their ports.
 * @author Mustafa Garip & Sushi Systems
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/rig_graph.hpp>
#include <SushiDSP/nodes/room/room_arrangement.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>

#include <SushiDSP/nodes/room_scenes/room_fixtures.hpp>
#include <SushiDSP/nodes/room_scenes/room_ports.hpp>

using SushiDSP::Room::Arrangement;
using SushiDSP::room::RoomNode;

namespace
{
    constexpr double K_RATE = 48000.0;
    constexpr std::uint32_t K_BLOCK = 256;
    constexpr std::uint32_t K_BLOCKS = 40;
    constexpr std::uint32_t K_PORTS = RoomNode::K_OUTPUT_PORTS;

    /// @brief Holds every port of one render: [0] and [1] the listener's ears, [2 + m]
    /// microphone m.
    using Render = std::array<std::vector<float>, 2 + RoomNode::K_MAX_MICROPHONES>;

    /// @brief Returns a deterministic test signal for one cabinet: a click and a decaying tone
    /// whose pitch depends on the cabinet, so two cabinets never carry the same block.
    std::vector<float> cabinet_signal(std::uint32_t cabinet)
    {
        std::vector<float> signal(static_cast<std::size_t>(K_BLOCK) * K_BLOCKS, 0.0f);
        const double hz = 180.0 + 140.0 * static_cast<double>(cabinet);
        for (std::size_t frame = 0; frame < signal.size(); ++frame)
        {
            const double t = static_cast<double>(frame) / K_RATE;
            signal[frame] = static_cast<float>(0.4 * std::exp(-6.0 * t) *
                                               std::sin(2.0 * 3.14159265358979 * hz * t));
        }
        signal[7 + cabinet] += 0.5f;
        return signal;
    }

    /// @brief Returns a prepared node holding one arrangement.
    std::unique_ptr<RoomNode> prepared(const Arrangement& arrangement)
    {
        auto node = std::make_unique<RoomNode>();
        node->prepare(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 2});
        std::string reason;
        EXPECT_TRUE(node->set_arrangement(arrangement, reason)) << reason;
        node->reset();
        return node;
    }

    /// @brief Renders K_BLOCKS blocks through a node; cabinet s plays cabinet_signal(s) when
    /// playing[s] is set and silence otherwise.
    Render render(RoomNode& node, const std::array<bool, 3>& playing)
    {
        const std::size_t frames = static_cast<std::size_t>(K_BLOCK) * K_BLOCKS;
        std::array<std::vector<float>, 3> signals;
        for (std::uint32_t cabinet = 0; cabinet < 3; ++cabinet)
        {
            signals[cabinet] =
                playing[cabinet] ? cabinet_signal(cabinet) : std::vector<float>(frames, 0.0f);
        }
        Render result;
        for (std::vector<float>& port : result)
        {
            port.assign(frames, 0.0f);
        }
        SushiDSP::test::RoomPorts ports(K_BLOCK);
        for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
        {
            const std::size_t offset = static_cast<std::size_t>(block) * K_BLOCK;
            const float* inputs[6] = {signals[0].data() + offset, signals[0].data() + offset,
                                      signals[1].data() + offset, signals[1].data() + offset,
                                      signals[2].data() + offset, signals[2].data() + offset};
            SushiDSP::ProcessContext context = ports.context(inputs, K_BLOCK);
            node.process(context);
            std::copy(ports.listener(0).begin(), ports.listener(0).end(),
                      result[0].begin() + static_cast<std::ptrdiff_t>(offset));
            std::copy(ports.listener(1).begin(), ports.listener(1).end(),
                      result[1].begin() + static_cast<std::ptrdiff_t>(offset));
            for (std::uint32_t microphone = 0; microphone < RoomNode::K_MAX_MICROPHONES;
                 ++microphone)
            {
                std::copy(ports.microphone(microphone).begin(), ports.microphone(microphone).end(),
                          result[2 + microphone].begin() + static_cast<std::ptrdiff_t>(offset));
            }
        }
        return result;
    }

    /// @brief Returns the largest magnitude of one block of samples.
    float peak(const std::vector<float>& samples)
    {
        float worst = 0.0f;
        for (const float sample : samples)
        {
            worst = std::max(worst, std::fabs(sample));
        }
        return worst;
    }

    /// @brief Returns the largest |a[n] - b[n]|.
    float peak_difference(const std::vector<float>& a, const std::vector<float>& b)
    {
        float worst = 0.0f;
        for (std::size_t frame = 0; frame < a.size(); ++frame)
        {
            worst = std::max(worst, std::fabs(a[frame] - b[frame]));
        }
        return worst;
    }

    /// @brief A source of one constant level, for the graph test.
    class LevelSourceNode : public SushiDSP::AudioNode
    {
        public:
            explicit LevelSourceNode(float level) : level_(level) {}
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext& context) override
            {
                // An impulse on the first block, then silence, so the room's response decays.
                std::fill_n(context.outputs[0], context.frames, 0.0f);
                if (!fired_)
                {
                    context.outputs[0][0] = level_;
                    fired_ = true;
                }
            }
            SushiDSP::PortLayout ports() const override { return {0, 1}; }
            void reset() override {}
            float get_parameter(const char*) const override { return 0.0f; }
            void set_parameter_for_control_thread(const char*, float) override {}

        private:
            float level_;
            bool fired_ = false;
    };

    /// @brief A mono consumer that records the largest magnitude it has been handed.
    class PeakSinkNode : public SushiDSP::AudioNode
    {
        public:
            void prepare(const SushiDSP::PrepareInfo&) override {}
            void process(SushiDSP::ProcessContext& context) override
            {
                for (std::uint32_t frame = 0; frame < context.frames; ++frame)
                {
                    const float sample = context.inputs[0] != nullptr ? context.inputs[0][frame] : 0.0f;
                    peak_ = std::max(peak_, std::fabs(sample));
                    context.outputs[0][frame] = sample;
                }
            }
            SushiDSP::PortLayout ports() const override { return {1, 1}; }
            void reset() override {}
            float get_parameter(const char*) const override { return 0.0f; }
            void set_parameter_for_control_thread(const char*, float) override {}

            float peak_ = 0.0f;
    };
} // namespace

TEST(EnvironmentRenderSet, RoomEnvironmentListenerPortCarriesWhatTheBareNodeRenders)
{
    // One cabinet, one receiver: the port table changes where the block lands, not the block.
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 0);
    std::unique_ptr<RoomNode> through_ports = prepared(arrangement);
    std::unique_ptr<RoomNode> bare = prepared(arrangement);

    const Render ported = render(*through_ports, {true, false, false});

    const std::vector<float> signal = cabinet_signal(0);
    std::vector<float> left(K_BLOCK, 0.0f);
    std::vector<float> right(K_BLOCK, 0.0f);
    float* outputs[2] = {left.data(), right.data()};
    float worst = 0.0f;
    for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
    {
        const std::size_t offset = static_cast<std::size_t>(block) * K_BLOCK;
        const float* inputs[1] = {signal.data() + offset};
        SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
        bare->process(context);
        for (std::uint32_t frame = 0; frame < K_BLOCK; ++frame)
        {
            worst = std::max(worst, std::fabs(left[frame] - ported[0][offset + frame]));
            worst = std::max(worst, std::fabs(right[frame] - ported[1][offset + frame]));
        }
    }

    EXPECT_EQ(worst, 0.0f);
    EXPECT_GT(peak(ported[0]), 1e-4f);
}

TEST(EnvironmentRenderSet, RoomEnvironmentRendersTheSumOfItsCabinets)
{
    // Two cabinets and three receivers: every port equals the sum of each cabinet played alone.
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);
    std::unique_ptr<RoomNode> both = prepared(arrangement);
    std::unique_ptr<RoomNode> first = prepared(arrangement);
    std::unique_ptr<RoomNode> second = prepared(arrangement);

    const Render together = render(*both, {true, true, false});
    const Render alone_first = render(*first, {true, false, false});
    const Render alone_second = render(*second, {false, true, false});

    for (std::uint32_t port = 0; port < 4; ++port)
    {
        std::vector<float> sum(together[port].size());
        for (std::size_t frame = 0; frame < sum.size(); ++frame)
        {
            sum[frame] = alone_first[port][frame] + alone_second[port][frame];
        }
        const float level = peak(together[port]);
        ASSERT_GT(level, 1e-4f) << "port " << port;
        EXPECT_LT(peak_difference(together[port], sum), 2e-5f * std::max(level, 1.0f))
            << "port " << port;
    }
    // The two cabinets stand in different places, so neither alone is the whole.
    EXPECT_GT(peak_difference(together[2], alone_first[2]), 1e-4f);
}

TEST(EnvironmentRenderSet, RoomEnvironmentHearsEachMicrophoneTheSameWhateverElseIsPlaced)
{
    // A pair is solved from its own cabinet and its own receiver alone.
    const SushiDSP::room::RoomScene shell = SushiDSP::test::garage_scene();
    const Arrangement crowded = SushiDSP::test::spread_arrangement(shell, 1, 3);
    Arrangement alone = SushiDSP::test::spread_arrangement(shell, 1, 0);
    std::string reason;
    ASSERT_TRUE(alone.add_microphone(crowded.microphones()[2], reason)) << reason;

    std::unique_ptr<RoomNode> crowded_node = prepared(crowded);
    std::unique_ptr<RoomNode> alone_node = prepared(alone);
    const Render in_crowd = render(*crowded_node, {true, false, false});
    const Render by_itself = render(*alone_node, {true, false, false});

    // Microphone 2 of the crowded room is microphone 0 of the other.
    ASSERT_GT(peak(by_itself[2]), 1e-4f);
    EXPECT_LT(peak_difference(in_crowd[4], by_itself[2]), 1e-6f);
    EXPECT_LT(peak_difference(in_crowd[0], by_itself[0]), 1e-6f);
}

TEST(EnvironmentRenderSet, RoomEnvironmentIsSilentWithNoCabinet)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 0, 3);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    const Render result = render(*node, {true, true, true});

    for (std::uint32_t port = 0; port < result.size(); ++port)
    {
        EXPECT_EQ(peak(result[port]), 0.0f) << "port " << port;
    }
}

TEST(EnvironmentRenderSet, RoomEnvironmentLeavesAPortWithNoMicrophoneSilent)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 2);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    const Render result = render(*node, {true, false, false});

    EXPECT_GT(peak(result[2]), 1e-4f);
    EXPECT_GT(peak(result[3]), 1e-4f);
    for (std::uint32_t port = 4; port < result.size(); ++port)
    {
        EXPECT_EQ(peak(result[port]), 0.0f) << "port " << port;
    }
}

TEST(EnvironmentRenderSet, RoomEnvironmentHearsAMovedMicrophoneAtTheNextPublish)
{
    const SushiDSP::room::RoomScene shell = SushiDSP::test::garage_scene();
    Arrangement arrangement = SushiDSP::test::spread_arrangement(shell, 1, 1);
    std::unique_ptr<RoomNode> moved = prepared(arrangement);
    std::unique_ptr<RoomNode> steady = prepared(arrangement);

    // Before the publish both nodes render the same block.
    const Render before_moved = render(*moved, {true, false, false});
    const Render before_steady = render(*steady, {true, false, false});
    EXPECT_EQ(peak_difference(before_moved[2], before_steady[2]), 0.0f);

    SushiDSP::Room::MicrophonePlacement microphone = arrangement.microphones()[0];
    microphone.pose.position.x += 1.5;
    ASSERT_TRUE(arrangement.update_microphone(microphone));
    std::string reason;
    ASSERT_TRUE(moved->set_arrangement(arrangement, reason)) << reason;

    const Render after_moved = render(*moved, {true, false, false});
    const Render after_steady = render(*steady, {true, false, false});
    EXPECT_GT(peak_difference(after_moved[2], after_steady[2]), 1e-4f);
    // The listener did not move, and its pairs were not solved again.
    EXPECT_LT(peak_difference(after_moved[0], after_steady[0]), 1e-6f);
}

TEST(EnvironmentRenderSet, RoomEnvironmentRefusesAnArrangementOutsideTheShellAndKeepsTheOldOne)
{
    Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 1);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    SushiDSP::Room::MicrophonePlacement outside = arrangement.microphones()[0];
    outside.pose.position.z = 40.0;
    ASSERT_TRUE(arrangement.update_microphone(outside));
    std::string reason;

    EXPECT_FALSE(node->set_arrangement(arrangement, reason));
    EXPECT_FALSE(reason.empty());
    EXPECT_LT(node->arrangement().microphones()[0].pose.position.z, 3.0);
}

TEST(EnvironmentRenderSet, RoomEnvironmentSetSceneKeepsTheMicrophonesAndMovesTheFirstCabinet)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    SushiDSP::room::RoomScene scene = node->scene();
    scene.source_position = {2.5, 1.0, 0.6};
    scene.listener_position = {5.0, 3.0, 1.3};
    std::string reason;
    ASSERT_TRUE(node->set_scene(scene, reason)) << reason;

    const Arrangement after = node->arrangement();
    ASSERT_EQ(after.sources().size(), 2u);
    EXPECT_EQ(after.sources()[0].pose.position.x, 2.5);
    EXPECT_EQ(after.sources()[1].pose.position.y, arrangement.sources()[1].pose.position.y);
    EXPECT_EQ(after.listener().position.x, 5.0);
    EXPECT_EQ(after.microphones().size(), 2u);
    EXPECT_EQ(node->scene().source_position.x, 2.5);
}

TEST(EnvironmentRenderSet, RoomEnvironmentRendersEveryPortThroughARigGraph)
{
    // The listener's port is the graph's output; each microphone port feeds its own consumer.
    SushiDSP::RigGraph graph(K_BLOCK, 64);
    const auto room = graph.add_node(std::make_unique<RoomNode>());
    for (int cabinet = 0; cabinet < 3; ++cabinet)
    {
        graph.connect(graph.add_node(std::make_unique<LevelSourceNode>(0.5f)), room);
    }
    std::array<PeakSinkNode*, RoomNode::K_MAX_MICROPHONES> sinks{};
    // The consumers join the listener's port in a last node, so the graph has one sink.
    auto join = std::make_unique<PeakSinkNode>();
    PeakSinkNode* const join_raw = join.get();
    const auto join_handle = graph.add_node(std::move(join));
    graph.connect(room, join_handle, RoomNode::K_LISTENER_PORT);
    for (std::uint32_t microphone = 0; microphone < 7; ++microphone)
    {
        auto sink = std::make_unique<PeakSinkNode>();
        sinks[microphone] = sink.get();
        const auto handle = graph.add_node(std::move(sink));
        graph.connect(room, handle, RoomNode::microphone_port(microphone));
        graph.connect(handle, join_handle);
    }
    ASSERT_EQ(K_PORTS, 9u);
    graph.compile(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 1});

    auto* const node = static_cast<RoomNode*>(graph.node(room));
    std::string reason;
    ASSERT_TRUE(node->set_arrangement(
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 3, 7), reason))
        << reason;

    std::vector<float> out(K_BLOCK, 0.0f);
    float* outputs[1] = {out.data()};
    for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
    {
        SushiDSP::ProcessContext context{outputs, nullptr, K_BLOCK, 0.0};
        graph.process(context);
    }

    EXPECT_GT(join_raw->peak_, 1e-5f);
    for (std::uint32_t microphone = 0; microphone < 7; ++microphone)
    {
        EXPECT_GT(sinks[microphone]->peak_, 1e-5f) << "microphone " << microphone;
    }
}

TEST(EnvironmentRenderSet, RoomShellDesignDoesNotReadThePoses)
{
    const SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    SushiDSP::room::RoomScene moved = scene;
    moved.source_position.x += 0.4;
    moved.listener_position.y -= 0.3;
    moved.microphone_position.z += 0.2;

    const SushiDSP::room::ShellDesign here = SushiDSP::room::design_shell(scene, 48000.0);
    const SushiDSP::room::ShellDesign there = SushiDSP::room::design_shell(moved, 48000.0);

    EXPECT_EQ(here.network.design.line_frames, there.network.design.line_frames);
    EXPECT_EQ(here.network.energy, there.network.energy);
    ASSERT_EQ(here.modal.design.count, there.modal.design.count);
    ASSERT_GT(here.modal.design.count, 0u);
    for (std::uint32_t mode = 0; mode < here.modal.design.count; ++mode)
    {
        EXPECT_EQ(here.modal.design.modes[mode].section, there.modal.design.modes[mode].section);
    }
}
