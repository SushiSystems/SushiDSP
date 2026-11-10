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

    /// @brief Counts the microphones the rig-graph test places.
    constexpr std::uint32_t K_RIG_MICROPHONES = 7;

    /// @brief Names a port as sound when its peak exceeds this. Characterised, kept from the
    /// original test.
    constexpr float K_AUDIBLE = 1e-4f;

    /// @brief Names a port of the rig graph as sound when its peak exceeds this. Characterised,
    /// kept from the original test.
    constexpr float K_RIG_AUDIBLE = 1e-5f;

    /// @brief Bounds the difference between a render of two cabinets together and the sum of each
    /// alone, as a share of the larger of the render's peak and one. Characterised: the cabinets
    /// pass through recursive filters whose rounding does not distribute over the sum. Kept from
    /// the original test.
    constexpr float K_SUPERPOSITION_SHARE = 2e-5f;

    /// @brief Bounds the difference between two renders that must agree. Characterised, kept
    /// from the original test.
    constexpr float K_ISOLATION_BOUND = 1e-6f;

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

    /// @brief Renders cabinet 0 alone through a node used directly, with no port table, and
    /// returns its left and right ears.
    std::array<std::vector<float>, 2> render_bare(RoomNode& node)
    {
        const std::vector<float> signal = cabinet_signal(0);
        std::array<std::vector<float>, 2> ears;
        std::vector<float> left(K_BLOCK, 0.0f);
        std::vector<float> right(K_BLOCK, 0.0f);
        float* outputs[2] = {left.data(), right.data()};
        for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
        {
            const std::size_t offset = static_cast<std::size_t>(block) * K_BLOCK;
            const float* inputs[1] = {signal.data() + offset};
            SushiDSP::ProcessContext context{outputs, inputs, K_BLOCK, 0.0};
            node.process(context);
            ears[0].insert(ears[0].end(), left.begin(), left.end());
            ears[1].insert(ears[1].end(), right.begin(), right.end());
        }
        return ears;
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

    /// @brief Expects the first four ports of a render of two cabinets together to equal the sum
    /// of the two cabinets rendered alone, within K_SUPERPOSITION_SHARE of the render's peak.
    void expect_ports_are_the_sum(const Render& together, const Render& first, const Render& second)
    {
        for (std::uint32_t port = 0; port < 4; ++port)
        {
            std::vector<float> sum(together[port].size());
            for (std::size_t frame = 0; frame < sum.size(); ++frame)
            {
                sum[frame] = first[port][frame] + second[port][frame];
            }
            const float level = peak(together[port]);
            ASSERT_GT(level, K_AUDIBLE) << "port " << port;
            EXPECT_LT(peak_difference(together[port], sum), K_SUPERPOSITION_SHARE * std::max(level, 1.0f))
                << "port " << port;
        }
    }

    /// @brief Expects every port of a render from @p first on to carry exactly nothing.
    void expect_ports_silent_from(const Render& result, std::uint32_t first)
    {
        for (std::uint32_t port = first; port < result.size(); ++port)
        {
            EXPECT_EQ(peak(result[port]), 0.0f) << "port " << port;
        }
    }

    /// @brief Holds the four renders the moved-microphone tests compare.
    struct MovedRenders
    {
            /// @brief Stores the node that moves, before the publish.
            Render before_moved;

            /// @brief Stores the node that stays, before the publish.
            Render before_steady;

            /// @brief Stores the node that moves, after the publish.
            Render after_moved;

            /// @brief Stores the node that stays, after the publish.
            Render after_steady;
    };

    /// @brief Renders two identical nodes, moves the first one's microphone by 1.5 m, publishes
    /// it, and renders both again.
    MovedRenders render_moved_microphone()
    {
        const SushiDSP::room::RoomScene shell = SushiDSP::test::garage_scene();
        Arrangement arrangement = SushiDSP::test::spread_arrangement(shell, 1, 1);
        std::unique_ptr<RoomNode> moved = prepared(arrangement);
        std::unique_ptr<RoomNode> steady = prepared(arrangement);
        MovedRenders renders;
        renders.before_moved = render(*moved, {true, false, false});
        renders.before_steady = render(*steady, {true, false, false});

        SushiDSP::Room::MicrophonePlacement microphone = arrangement.microphones()[0];
        microphone.pose.position.x += 1.5;
        EXPECT_TRUE(arrangement.update_microphone(microphone));
        std::string reason;
        EXPECT_TRUE(moved->set_arrangement(arrangement, reason)) << reason;

        renders.after_moved = render(*moved, {true, false, false});
        renders.after_steady = render(*steady, {true, false, false});
        return renders;
    }

    /// @brief Returns the room node after set_scene moved its first cabinet and its listener.
    Arrangement arrangement_after_set_scene(const Arrangement& before)
    {
        std::unique_ptr<RoomNode> node = prepared(before);
        SushiDSP::room::RoomScene scene = node->scene();
        scene.source_position = {2.5, 1.0, 0.6};
        scene.listener_position = {5.0, 3.0, 1.3};
        std::string reason;
        EXPECT_TRUE(node->set_scene(scene, reason)) << reason;
        return node->arrangement();
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

    /// @brief Holds a rig graph around a room node: three level sources feed the room, the
    /// listener's port feeds a join, and each microphone port feeds its own consumer.
    struct RigScene
    {
            /// @brief Stores the graph.
            SushiDSP::RigGraph graph{K_BLOCK, 64};

            /// @brief Stores the room's handle.
            SushiDSP::NodeHandle room{};

            /// @brief Points at the join the listener's port feeds.
            PeakSinkNode* join = nullptr;

            /// @brief Points at each microphone's consumer.
            std::array<PeakSinkNode*, RoomNode::K_MAX_MICROPHONES> sinks{};
    };

    /// @brief Builds the rig graph, compiles it and hands the room an arrangement of three
    /// cabinets and seven microphones.
    std::unique_ptr<RigScene> make_rig_scene()
    {
        auto scene = std::make_unique<RigScene>();
        scene->room = scene->graph.add_node(std::make_unique<RoomNode>());
        for (int cabinet = 0; cabinet < 3; ++cabinet)
        {
            scene->graph.connect(scene->graph.add_node(std::make_unique<LevelSourceNode>(0.5f)), scene->room);
        }
        auto join = std::make_unique<PeakSinkNode>();
        scene->join = join.get();
        const auto join_handle = scene->graph.add_node(std::move(join));
        scene->graph.connect(scene->room, join_handle, RoomNode::K_LISTENER_PORT);
        for (std::uint32_t microphone = 0; microphone < K_RIG_MICROPHONES; ++microphone)
        {
            auto sink = std::make_unique<PeakSinkNode>();
            scene->sinks[microphone] = sink.get();
            const auto handle = scene->graph.add_node(std::move(sink));
            scene->graph.connect(scene->room, handle, RoomNode::microphone_port(microphone));
            scene->graph.connect(handle, join_handle);
        }
        scene->graph.compile(SushiDSP::PrepareInfo{K_RATE, K_BLOCK, 1});
        std::string reason;
        auto* const node = static_cast<RoomNode*>(scene->graph.node(scene->room));
        EXPECT_TRUE(node->set_arrangement(
            SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 3, K_RIG_MICROPHONES), reason))
            << reason;
        return scene;
    }

    /// @brief Runs K_BLOCKS blocks through the rig graph.
    void run_rig(RigScene& scene)
    {
        std::vector<float> out(K_BLOCK, 0.0f);
        float* outputs[1] = {out.data()};
        for (std::uint32_t block = 0; block < K_BLOCKS; ++block)
        {
            SushiDSP::ProcessContext context{outputs, nullptr, K_BLOCK, 0.0};
            scene.graph.process(context);
        }
    }

    /// @brief Expects every microphone consumer of the rig to have been handed sound.
    void expect_every_microphone_consumer_heard(const RigScene& scene)
    {
        for (std::uint32_t microphone = 0; microphone < K_RIG_MICROPHONES; ++microphone)
        {
            EXPECT_GT(scene.sinks[microphone]->peak_, K_RIG_AUDIBLE) << "microphone " << microphone;
        }
    }

    /// @brief Expects the modal sections of two shell designs to be the same, mode by mode.
    void expect_same_modal_sections(const SushiDSP::room::ShellDesign& a, const SushiDSP::room::ShellDesign& b)
    {
        for (std::uint32_t mode = 0; mode < a.modal.design.count; ++mode)
        {
            EXPECT_EQ(a.modal.design.modes[mode].section, b.modal.design.modes[mode].section) << "mode " << mode;
        }
    }
} // namespace

TEST(Small_EnvironmentRenderSet, RoomEnvironmentListenerPortCarriesWhatTheBareNodeRenders)
{
    // The same arrangement and input run the same arithmetic, so the renders agree bit for bit.
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 0);
    std::unique_ptr<RoomNode> through_ports = prepared(arrangement);
    std::unique_ptr<RoomNode> bare = prepared(arrangement);

    const Render ported = render(*through_ports, {true, false, false});
    const std::array<std::vector<float>, 2> ears = render_bare(*bare);

    EXPECT_EQ(ported[0], ears[0]);
    EXPECT_EQ(ported[1], ears[1]);
    EXPECT_GT(peak(ported[0]), K_AUDIBLE);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentRendersTheSumOfItsCabinets)
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

    expect_ports_are_the_sum(together, alone_first, alone_second);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentHearsTheTwoCabinetsInDifferentPlaces)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);
    std::unique_ptr<RoomNode> both = prepared(arrangement);
    std::unique_ptr<RoomNode> first = prepared(arrangement);

    const Render together = render(*both, {true, true, false});
    const Render alone_first = render(*first, {true, false, false});

    // The two cabinets stand in different places, so neither alone is the whole.
    EXPECT_GT(peak_difference(together[2], alone_first[2]), K_AUDIBLE);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentHearsEachMicrophoneTheSameWhateverElseIsPlaced)
{
    // A pair is solved from its own cabinet and receiver alone, so the same arithmetic gives the same block.
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
    ASSERT_GT(peak(by_itself[2]), K_AUDIBLE);
    EXPECT_LT(peak_difference(in_crowd[4], by_itself[2]), K_ISOLATION_BOUND);
    EXPECT_LT(peak_difference(in_crowd[0], by_itself[0]), K_ISOLATION_BOUND);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentIsSilentWithNoCabinet)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 0, 3);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    const Render result = render(*node, {true, true, true});

    expect_ports_silent_from(result, 0u);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentCarriesSoundOnAPortWithAMicrophone)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 2);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    const Render result = render(*node, {true, false, false});

    EXPECT_GT(peak(result[2]), K_AUDIBLE);
    EXPECT_GT(peak(result[3]), K_AUDIBLE);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentLeavesAPortWithNoMicrophoneSilent)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 1, 2);
    std::unique_ptr<RoomNode> node = prepared(arrangement);

    const Render result = render(*node, {true, false, false});

    expect_ports_silent_from(result, 4u);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentHearsAMovedMicrophoneAtTheNextPublish)
{
    const MovedRenders renders = render_moved_microphone();

    // Before the publish both nodes render the same block, bit for bit, from the same arithmetic.
    ASSERT_EQ(peak_difference(renders.before_moved[2], renders.before_steady[2]), 0.0f);
    EXPECT_GT(peak_difference(renders.after_moved[2], renders.after_steady[2]), K_AUDIBLE);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentLeavesTheListenerUnchangedWhenAMicrophoneMoves)
{
    // The listener did not move, and its pairs were not solved again.
    const MovedRenders renders = render_moved_microphone();

    EXPECT_LT(peak_difference(renders.after_moved[0], renders.after_steady[0]), K_ISOLATION_BOUND);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentRefusesAnArrangementOutsideTheShellAndKeepsTheOldOne)
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

TEST(Small_EnvironmentRenderSet, RoomEnvironmentSetSceneMovesTheFirstCabinet)
{
    const Arrangement before = SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);

    const Arrangement after = arrangement_after_set_scene(before);

    ASSERT_EQ(after.sources().size(), 2u);
    EXPECT_EQ(after.sources()[0].pose.position.x, 2.5);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentSetSceneKeepsTheSecondCabinet)
{
    const Arrangement before = SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);

    const Arrangement after = arrangement_after_set_scene(before);

    ASSERT_EQ(after.sources().size(), 2u);
    EXPECT_EQ(after.sources()[1].pose.position.y, before.sources()[1].pose.position.y);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentSetSceneKeepsTheMicrophones)
{
    const Arrangement before = SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);

    const Arrangement after = arrangement_after_set_scene(before);

    EXPECT_EQ(after.microphones().size(), 2u);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentSetSceneMovesTheListener)
{
    const Arrangement before = SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);

    const Arrangement after = arrangement_after_set_scene(before);

    EXPECT_EQ(after.listener().position.x, 5.0);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentSetSceneReadsBackTheFirstCabinetInTheScene)
{
    const Arrangement arrangement =
        SushiDSP::test::spread_arrangement(SushiDSP::test::garage_scene(), 2, 2);
    std::unique_ptr<RoomNode> node = prepared(arrangement);
    SushiDSP::room::RoomScene scene = node->scene();
    scene.source_position = {2.5, 1.0, 0.6};
    std::string reason;
    ASSERT_TRUE(node->set_scene(scene, reason)) << reason;

    EXPECT_EQ(node->scene().source_position.x, 2.5);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentRendersTheListenerPortThroughARigGraph)
{
    std::unique_ptr<RigScene> scene = make_rig_scene();

    run_rig(*scene);

    EXPECT_GT(scene->join->peak_, K_RIG_AUDIBLE);
}

TEST(Small_EnvironmentRenderSet, RoomEnvironmentRendersEveryMicrophonePortThroughARigGraph)
{
    std::unique_ptr<RigScene> scene = make_rig_scene();

    run_rig(*scene);

    expect_every_microphone_consumer_heard(*scene);
}

TEST(Small_EnvironmentRenderSet, RoomShellDesignDoesNotReadThePoses)
{
    // The design is the same arithmetic on the same room, so the two agree bit for bit.
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
    expect_same_modal_sections(here, there);
}
