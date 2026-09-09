/**************************************************************************/
/* test_room_node.cpp                                                     */
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
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_room_node.cpp
 * @brief Tests for RoomNode: its ports, its tap placement, its crossfade, its swap and its mix.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "RoomFixtures.hpp"
#include "SushiDSP/core/IAudioNode.hpp"
#include "SushiDSP/node_registry/NodeRegistry.hpp"
#include "SushiDSP/room/CabinetDirectivity.hpp"
#include "SushiDSP/room/ImageSourceSolver.hpp"
#include "SushiDSP/room/RoomNode.hpp"
#include "SushiDSP/room/RoomRenderSet.hpp"
#include "SushiDSP/room/SphericalHeadHrtf.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Holds the sample rate every node in this file runs at.
    constexpr double kSampleRate = 48000.0;

    /// @brief Holds the block length every node in this file renders in.
    constexpr std::uint32_t kBlock = 256;

    /// @brief Holds the frames one metre of path takes at kSampleRate and 343 m/s.
    constexpr double kFramesPerMetre = kSampleRate / 343.0;

    /// @brief Holds the PrepareInfo every node in this file is prepared with.
    constexpr SushiDSP::PrepareInfo kInfo{ kSampleRate, kBlock, 1 };

    /// @brief Returns the garage with the source at (1.5, 2.0, 0.5) and an omni microphone at
    /// (4.0, 2.0, 1.2), the listener spot of test_image_source_solver.cpp's example scene.
    SushiDSP::room::RoomScene microphone_scene()
    {
        SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
        scene.source_position = { 1.5, 2.0, 0.5 };
        scene.microphone_position = { 4.0, 2.0, 1.2 };
        scene.microphone_pattern = SushiDSP::room::PolarPattern::Omni;
        return scene;
    }

    /// @brief Returns a prepared node at full wet mix, unity level, with one scene in place.
    std::unique_ptr<SushiDSP::room::RoomNode> prepared_node(const SushiDSP::room::RoomScene& scene)
    {
        auto node = std::make_unique<SushiDSP::room::RoomNode>();
        node->set_parameter("mix", 1.0f);
        node->prepare(kInfo);
        std::string reason;
        EXPECT_TRUE(node->set_scene(scene, reason)) << reason;
        return node;
    }

    /// @brief Runs one input through a node block by block and returns the left channel.
    std::vector<float> render(SushiDSP::room::RoomNode& node, const std::vector<float>& input,
                              const std::vector<std::uint32_t>& swap_blocks = {},
                              const SushiDSP::room::RoomScene* swap_scene = nullptr)
    {
        std::vector<float> output(input.size(), 0.0f);
        std::vector<float> left(kBlock, 0.0f);
        std::vector<float> right(kBlock, 0.0f);
        float* outputs[2] = { left.data(), right.data() };
        for (std::size_t offset = 0; offset + kBlock <= input.size(); offset += kBlock)
        {
            const std::uint32_t block = static_cast<std::uint32_t>(offset / kBlock);
            for (std::uint32_t swap : swap_blocks)
            {
                if (swap == block && swap_scene != nullptr)
                {
                    std::string reason;
                    EXPECT_TRUE(node.set_scene(*swap_scene, reason)) << reason;
                }
            }
            const float* inputs[1] = { input.data() + offset };
            SushiDSP::ProcessContext context{ outputs, inputs, kBlock, 0.0 };
            node.process(context);
            std::memcpy(output.data() + offset, left.data(), kBlock * sizeof(float));
        }
        return output;
    }

    /// @brief Returns a 1 kHz full-scale tone of the given length.
    std::vector<float> tone(std::size_t frames)
    {
        std::vector<float> samples(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = static_cast<float>(
                std::sin(2.0 * kPi * 1000.0 * static_cast<double>(frame) / kSampleRate));
        }
        return samples;
    }

    /// @brief Returns the arriving set's weight the node uses at one frame of its fade.
    double fade_weight(std::size_t frame)
    {
        const double phase = static_cast<double>(frame + 1)
            / static_cast<double>(SushiDSP::room::RoomNode::kCrossfadeFrames);
        return 0.5 * (1.0 - std::cos(kPi * phase));
    }

    /// @brief Returns the index of the local maximum of |ir| nearest a frame, or -1 when no
    /// local maximum above the floor lies within the window.
    long nearest_peak(const std::vector<float>& ir, double frame, long window, float floor)
    {
        long best = -1;
        double best_distance = 1e9;
        const long low = std::max<long>(1, static_cast<long>(std::floor(frame)) - window);
        const long high = std::min<long>(static_cast<long>(ir.size()) - 2,
                                         static_cast<long>(std::ceil(frame)) + window);
        for (long index = low; index <= high; ++index)
        {
            const float here = std::fabs(ir[static_cast<std::size_t>(index)]);
            const bool peak = here > std::fabs(ir[static_cast<std::size_t>(index - 1)])
                && here >= std::fabs(ir[static_cast<std::size_t>(index + 1)]) && here > floor;
            const double distance = std::fabs(static_cast<double>(index) - frame);
            if (peak && distance < best_distance)
            {
                best_distance = distance;
                best = index;
            }
        }
        return best;
    }

    /// @brief Returns the largest |a[n] - b[n]| over [begin, end).
    double peak_difference(const std::vector<float>& a, const std::vector<float>& b,
                           std::size_t begin, std::size_t end)
    {
        double worst = 0.0;
        for (std::size_t frame = begin; frame < end; ++frame)
        {
            worst = std::max(worst, static_cast<double>(std::fabs(a[frame] - b[frame])));
        }
        return worst;
    }

    /// @brief Returns 20 log10 of a linear value.
    double to_decibels(double value)
    {
        return 20.0 * std::log10(std::max(value, 1e-30));
    }

    /// @brief Holds what one publish under the crossfade measures.
    struct CrossfadeMeasure
    {
        /// @brief Stores the moving render's largest sample-to-sample step over the fade,
        /// divided by the reference's own.
        double step_ratio;

        /// @brief Stores the largest difference between the moving render and the reference
        /// from the swap to one block past the fade.
        double residual;
    };

    /// @brief Renders a tone through three nodes: one in scene a throughout, one in scene b
    /// throughout, and one that moves from a to b at block 200; the reference is the
    /// raised-cosine blend of the two steady renders over the node's fade.
    CrossfadeMeasure measure_crossfade(const SushiDSP::room::RoomScene& scene_a,
                                       const SushiDSP::room::RoomScene& scene_b)
    {
        constexpr std::uint32_t kSwapBlock = 200;
        constexpr std::size_t kFrames = 400 * kBlock;
        const std::vector<float> input = tone(kFrames);

        std::unique_ptr<SushiDSP::room::RoomNode> steady_a = prepared_node(scene_a);
        std::unique_ptr<SushiDSP::room::RoomNode> steady_b = prepared_node(scene_b);
        std::unique_ptr<SushiDSP::room::RoomNode> moving = prepared_node(scene_a);
        const std::vector<float> render_a = render(*steady_a, input);
        const std::vector<float> render_b = render(*steady_b, input);
        const std::vector<float> render_x = render(*moving, input, { kSwapBlock }, &scene_b);

        const std::size_t swap = static_cast<std::size_t>(kSwapBlock) * kBlock;
        const std::size_t fade_end = swap + SushiDSP::room::RoomNode::kCrossfadeFrames;
        std::vector<float> reference(kFrames);
        for (std::size_t frame = 0; frame < kFrames; ++frame)
        {
            double weight = 0.0;
            if (frame >= fade_end)
            {
                weight = 1.0;
            }
            else if (frame >= swap)
            {
                weight = fade_weight(frame - swap);
            }
            reference[frame] = static_cast<float>((1.0 - weight) * render_a[frame]
                                                  + weight * render_b[frame]);
        }
        EXPECT_EQ(peak_difference(render_x, render_a, 0, swap), 0.0);

        double own_step = 0.0;
        double moving_step = 0.0;
        for (std::size_t frame = swap - kBlock; frame < fade_end + kBlock; ++frame)
        {
            own_step = std::max(own_step, static_cast<double>(
                std::fabs(reference[frame] - reference[frame - 1])));
            moving_step = std::max(moving_step, static_cast<double>(
                std::fabs(render_x[frame] - render_x[frame - 1])));
        }
        return { moving_step / own_step,
                 peak_difference(render_x, reference, swap, fade_end + kBlock) };
    }
}

TEST(Unit_Dsp, RoomNodeDeclaresOneInputAndTwoOutputs)
{
    SushiDSP::room::RoomNode node;
    EXPECT_EQ(node.ports().input_channels, 1u);
    EXPECT_EQ(node.ports().output_channels, 2u);
    EXPECT_STREQ(node.type_id(), "sushidsp.room");
}

TEST(Unit_Dsp, RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid)
{
    // The seven first-order paths of the example scene, in metres: direct sqrt(2.5^2 + 0.7^2)
    // = 2.596151, floor sqrt(2.5^2 + 1.7^2) = 3.023243, ceiling sqrt(2.5^2 + 3.3^2) =
    // 4.140048, the two y walls sqrt(2.5^2 + 4^2 + 0.7^2) = 4.768648, -x sqrt(5.5^2 + 0.7^2)
    // = 5.544367 and +x sqrt(6.5^2 + 0.7^2) = 6.537584; times 48000 / 343 that is 363.310,
    // 423.078, 579.365, 667.333 (twice), 775.888 and 914.881 frames. The tail's input waits
    // for t_c = 5 * 2.5 / 343 s = 1749 frames, so nothing but taps reaches the first 1749
    // frames of the response. The cabinet is a 1 cm piston: the 4x12's directivity fit puts
    // the ceiling image's peak, 53 degrees above the baffle's axis, one frame late at 580,
    // which is the filter's group delay and not the tap's placement.
    SushiDSP::room::RoomScene scene = microphone_scene();
    scene.cabinet = SushiDSP::room::CabinetGeometry{ 0.01, 0.0, 0.05, 20000.0 };
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(scene);

    std::vector<float> impulse(2048, 0.0f);
    impulse[0] = 1.0f;
    const std::vector<float> response = render(*node, impulse);

    const std::array<double, 6> metres = {
        std::sqrt(2.5 * 2.5 + 0.7 * 0.7), std::sqrt(2.5 * 2.5 + 1.7 * 1.7),
        std::sqrt(2.5 * 2.5 + 3.3 * 3.3), std::sqrt(2.5 * 2.5 + 4.0 * 4.0 + 0.7 * 0.7),
        std::sqrt(5.5 * 5.5 + 0.7 * 0.7), std::sqrt(6.5 * 6.5 + 0.7 * 0.7)
    };
    const std::array<double, 6> plan_frames = { 363.3098, 423.0778, 579.3654, 667.3326,
                                                775.8880, 914.8805 };
    for (std::size_t path = 0; path < metres.size(); ++path)
    {
        const double frames = metres[path] * kFramesPerMetre;
        EXPECT_NEAR(frames, plan_frames[path], 1e-3) << "path " << path;
        const long peak = nearest_peak(response, frames, 8, 1e-3f);
        ASSERT_GE(peak, 0) << "no peak near " << frames;
        EXPECT_NEAR(static_cast<double>(peak), frames, 0.5) << "path " << path;
    }
}

TEST(Unit_Dsp, RoomNodeCrossfadesAPublishWithoutAStep)
{
    // The microphone moves 0.36 m, which reorders the taps and refits every filter, so the
    // arriving bank starts from zero. Measured: a step ratio of 1.0033 and a residual of
    // 0.0020, -53.9 dBFS, where the two-shelf fit measured 1.0051 and 0.0233, -32.7 dBFS.
    // Spec 5.3 and the README's crossfade section carry the figures.
    SushiDSP::room::RoomScene moved = microphone_scene();
    moved.microphone_position = { 3.7, 2.2, 1.2 };
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), moved);
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.01);
    EXPECT_LT(to_decibels(measure.residual), -50.0);
}

TEST(Unit_Dsp, RoomNodeCrossfadesASmallMoveFromTheCarriedState)
{
    // One centimetre moves every delay by under 1.4 frames; 78 of the 96 taps refit within
    // the match bound and carry their state, the other 18 start from zero. Measured: a step
    // ratio of 1.0000 and a residual of 0.0010, -60.2 dBFS, where the two-shelf fit carried 52
    // and measured 0.0160, -35.9 dBFS.
    SushiDSP::room::RoomScene nudged = microphone_scene();
    nudged.microphone_position.x += 0.01;
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), nudged);
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.01);
    EXPECT_LT(to_decibels(measure.residual), -58.0);
}

TEST(Unit_Dsp, RoomNodeRepublishesTheSameSceneWithoutAResidual)
{
    // The same scene refits to the same taps, every tap carries its state, and the arriving
    // bank renders the retiring bank's block sample for sample. Measured: a residual of
    // exactly zero.
    const CrossfadeMeasure measure = measure_crossfade(microphone_scene(), microphone_scene());
    RecordProperty("residual_dbfs", to_decibels(measure.residual));
    EXPECT_LE(measure.step_ratio, 1.0);
    EXPECT_LT(to_decibels(measure.residual), -120.0);
}

TEST(Unit_Dsp, RoomNodeKeepsRenderingTheOldSetWhileASetIsBuilt)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(microphone_scene());
    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float* outputs[2] = { left.data(), right.data() };
    std::vector<float> block(kBlock);

    std::size_t frame_count = 0;
    double quietest_rms = 1.0;
    for (int publish = 0; publish < 100; ++publish)
    {
        SushiDSP::room::RoomScene scene = microphone_scene();
        scene.microphone_position.x = 3.0 + 0.01 * publish;
        std::string reason;
        ASSERT_TRUE(node->set_scene(scene, reason)) << reason;

        for (int repeat = 0; repeat < 4; ++repeat)
        {
            for (std::uint32_t frame = 0; frame < kBlock; ++frame, ++frame_count)
            {
                block[frame] = static_cast<float>(std::sin(
                    2.0 * kPi * 1000.0 * static_cast<double>(frame_count) / kSampleRate));
            }
            const float* inputs[1] = { block.data() };
            SushiDSP::ProcessContext context{ outputs, inputs, kBlock, 0.0 };
            node->process(context);

            double energy = 0.0;
            for (float sample : left)
            {
                ASSERT_TRUE(std::isfinite(sample));
                energy += static_cast<double>(sample) * sample;
            }
            quietest_rms = std::min(quietest_rms, std::sqrt(energy / kBlock));
        }
        node->reclaim_rooms();
    }
    EXPECT_GT(to_decibels(quietest_rms), -80.0);
}

TEST(Unit_Dsp, RoomNodeReclaimsARetiredSetOnlyAfterTheAudioThreadMovedOn)
{
    // The node copies a set's taps into its own bank the block it first sees the set, and
    // never reads a slot pointer past that block, so a reclaim can free a retired set at any
    // time without a read from freed memory. test_rt_swap_slot.cpp proves the slot's own
    // deferral; this case walks the node through the same sequence and checks that the
    // block after each reclaim still carries the room.
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(microphone_scene());
    const std::vector<float> input = tone(4 * kBlock);

    SushiDSP::room::RoomScene second = microphone_scene();
    second.microphone_position.x = 3.5;
    SushiDSP::room::RoomScene third = microphone_scene();
    third.microphone_position.x = 3.0;

    // The direct tap arrives 363 frames in, so the room is silent for the first block and the
    // energy check starts at the second.
    const std::vector<float> first_blocks =
        render(*node, std::vector<float>(input.begin(), input.begin() + 2 * kBlock));
    const std::vector<float> first_block(first_blocks.begin() + kBlock, first_blocks.end());
    std::string reason;
    ASSERT_TRUE(node->set_scene(second, reason)) << reason;
    ASSERT_TRUE(node->set_scene(third, reason)) << reason;
    node->reclaim_rooms();

    const std::vector<float> after_reclaim = render(
        *node, std::vector<float>(input.begin() + 2 * kBlock, input.begin() + 3 * kBlock));
    node->reclaim_rooms();
    const std::vector<float> settled = render(
        *node, std::vector<float>(input.begin() + 3 * kBlock, input.begin() + 4 * kBlock));

    for (const std::vector<float>* block : { &first_block, &after_reclaim, &settled })
    {
        double energy = 0.0;
        for (float sample : *block)
        {
            ASSERT_TRUE(std::isfinite(sample));
            energy += static_cast<double>(sample) * sample;
        }
        EXPECT_GT(energy, 0.0);
    }
    EXPECT_EQ(node->scene().microphone_position.x, 3.0);
}

TEST(Unit_Dsp, RoomNodeRefusesAnInvalidSceneAndKeepsTheOldOne)
{
    std::unique_ptr<SushiDSP::room::RoomNode> node = prepared_node(microphone_scene());

    SushiDSP::room::RoomScene invalid = microphone_scene();
    invalid.dimensions.x = 15.0;
    std::string reason;
    EXPECT_FALSE(node->set_scene(invalid, reason));
    EXPECT_FALSE(reason.empty());
    EXPECT_EQ(node->scene().dimensions.x, 6.0);
    EXPECT_EQ(node->scene().microphone_position.x, 4.0);

    // Before prepare() the scene is still validated and refused the same way.
    SushiDSP::room::RoomNode fresh;
    EXPECT_FALSE(fresh.set_scene(invalid, reason));
    EXPECT_EQ(fresh.scene().dimensions.x, 6.0);
}

TEST(Unit_Dsp, RoomNodeMixesDryAndWet)
{
    // prepare() snaps the smoothers to their targets, so a mix set before it holds from the
    // first frame.
    SushiDSP::room::RoomNode dry;
    dry.set_parameter("mix", 0.0f);
    dry.prepare(kInfo);
    std::vector<float> input(2 * kBlock);
    for (std::size_t frame = 0; frame < input.size(); ++frame)
    {
        input[frame] = static_cast<float>(std::sin(0.1 * static_cast<double>(frame)));
    }
    std::vector<float> left(kBlock, 0.0f);
    std::vector<float> right(kBlock, 0.0f);
    float* outputs[2] = { left.data(), right.data() };
    for (std::size_t offset = 0; offset < input.size(); offset += kBlock)
    {
        const float* inputs[1] = { input.data() + offset };
        SushiDSP::ProcessContext context{ outputs, inputs, kBlock, 0.0 };
        dry.process(context);
        for (std::uint32_t frame = 0; frame < kBlock; ++frame)
        {
            EXPECT_EQ(left[frame], input[offset + frame]);
            EXPECT_EQ(right[frame], input[offset + frame]);
        }
    }

    // The default scene's microphone sits 0.1 m from the cabinet, 14 frames away, so a fully
    // wet node leaves the frames before that at zero: no dry path reaches the output.
    std::unique_ptr<SushiDSP::room::RoomNode> wet = prepared_node(SushiDSP::room::RoomScene{});
    std::vector<float> impulse(kBlock, 0.0f);
    impulse[0] = 1.0f;
    const std::vector<float> response = render(*wet, impulse);
    for (std::size_t frame = 0; frame < 12; ++frame)
    {
        EXPECT_EQ(response[frame], 0.0f) << "frame " << frame;
    }
    EXPECT_GT(std::fabs(response[14]), 0.0f);
}

TEST(Unit_Dsp, RoomNodeRemovesTheDirectImageForTheEarsAndKeepsItForTheMicrophone)
{
    // Tap 0 of a solved set is the (0, 0, 0) image, the shortest path. For the ears the HRIR
    // pair renders that path, so the published taps carry reflections only; the microphone's
    // direct sound is a tap and stays.
    const SushiDSP::room::HrtfSet hrtf = SushiDSP::room::SphericalHeadHrtf::build(kSampleRate);
    const SushiDSP::room::RoomScene scene = microphone_scene();

    std::unique_ptr<SushiDSP::room::RoomRenderSet> ears = SushiDSP::room::build_render_set(
        scene, hrtf, kSampleRate, SushiDSP::room::ListenerKind::Ears);
    const std::uint32_t solved = ears->early.count;
    const float direct = ears->early.taps[0].delay_frames;
    const float first_reflection = ears->early.taps[1].delay_frames;
    EXPECT_NEAR(direct, 363.3098f, 1e-2f);
    SushiDSP::room::RoomNode::remove_direct_image(*ears);
    EXPECT_EQ(ears->early.count, solved - 1u);
    EXPECT_FLOAT_EQ(ears->early.taps[0].delay_frames, first_reflection);
    for (std::uint32_t tap = 0; tap < ears->early.count; ++tap)
    {
        EXPECT_GT(ears->early.taps[tap].delay_frames, direct) << "tap " << tap;
    }

    std::unique_ptr<SushiDSP::room::RoomRenderSet> microphone =
        SushiDSP::room::build_render_set(scene, hrtf, kSampleRate,
                                         SushiDSP::room::ListenerKind::Microphone);
    const std::uint32_t before = microphone->early.count;
    SushiDSP::room::RoomNode::remove_direct_image(*microphone);
    EXPECT_EQ(microphone->early.count, before);
    EXPECT_NEAR(microphone->early.taps[0].delay_frames, 363.3098f, 1e-2f);
}

TEST(Unit_Dsp, RoomNodeIsRegisteredUnderRoom)
{
    bool found = false;
    for (const SushiDSP::NodeEntry& entry : SushiDSP::node_registry())
    {
        if (std::strcmp(entry.descriptor->id, "sushidsp.room") == 0)
        {
            found = true;
            EXPECT_STREQ(entry.category.name, "Room");
            EXPECT_EQ(entry.descriptor->parameter_count, 4u);
            EXPECT_STREQ(entry.descriptor->parameters[0].id, "mix");
            EXPECT_STREQ(entry.descriptor->parameters[1].id, "level");
            EXPECT_STREQ(entry.descriptor->parameters[2].id, "listener");
            EXPECT_STREQ(entry.descriptor->parameters[3].id, "direct_delay");
            EXPECT_FLOAT_EQ(entry.descriptor->parameters[3].default_value, 0.0f);
        }
    }
    EXPECT_TRUE(found);

    std::unique_ptr<SushiDSP::IAudioNode> node = SushiDSP::make_node("sushidsp.room", kInfo);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->ports().input_channels, 1u);
    EXPECT_EQ(node->ports().output_channels, 2u);
    EXPECT_FLOAT_EQ(node->get_parameter("mix"), 1.0f);
    EXPECT_FLOAT_EQ(node->get_parameter("level"), 0.0f);
    node->set_parameter("level", -6.0f);
    EXPECT_FLOAT_EQ(node->get_parameter("level"), -6.0f);
    EXPECT_NE(dynamic_cast<SushiDSP::room::IRoomHost*>(node.get()), nullptr);
}
