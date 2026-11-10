/**************************************************************************/
/* test_pair_stage.cpp                                                    */
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
 * @file test_pair_stage.cpp
 * @brief Tests that a pair stage goes live on adopt, forgets its set on reset splits an ears set's direct tap and renders for the receiver it was prepared for.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/delay_ring.hpp>
#include <SushiDSP/nodes/room/fade_window.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>
#include <SushiDSP/nodes/room/pair_stage.hpp>

#include <cstdint>
#include <cstddef>
#include <memory>
#include <numeric>
#include <vector>

namespace
{
    using SushiDSP::Room::PairStage;
    using SushiDSP::room::DelayRing;
    using SushiDSP::room::EarlyTap;
    using SushiDSP::room::EarlyTapSet;
    using SushiDSP::room::FadeWindow;
    using SushiDSP::room::ListenerKind;
    using SushiDSP::room::RoomRenderSet;

    constexpr double K_RATE = 48000.0;
    constexpr std::uint32_t K_ALIGNMENT_FRAMES = 128;
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds the gain tap 0 carries in the sets below.
    constexpr float K_DIRECT_GAIN = 0.5f;

    /// @brief Holds the gain the receiving direct tap carries before a microphone set is taken.
    constexpr float K_UNTOUCHED_GAIN = 7.0f;

    /// @brief Returns a set of the given listener with an empty tail and three early taps: the direct
    /// image of gain K_DIRECT_GAIN, then reflections at 10 and 20 frames.
    std::unique_ptr<RoomRenderSet> three_tap_set(ListenerKind listener)
    {
        auto set = std::make_unique<RoomRenderSet>();
        set->listener = listener;
        set->early.count = 3;
        set->early.taps[0].gain = K_DIRECT_GAIN;
        set->early.taps[1].delay_frames = 10.0f;
        set->early.taps[2].delay_frames = 20.0f;
        return set;
    }

    /// @brief Holds the frames the tail is read behind the block written.
    constexpr std::uint32_t K_TAIL_DELAY = 4;

    /// @brief Holds the longest delay the rings are sized for.
    constexpr std::uint32_t K_RING_FRAMES = 256;

    /// @brief Returns a set whose tail passes only the B sum: unit gain, K_TAIL_DELAY frames, the tone
    /// and the B coherence cascades the identity and the A coherence cascade silent.
    std::unique_ptr<RoomRenderSet> tail_b_set(ListenerKind listener)
    {
        auto set = three_tap_set(listener);
        set->late.input_gain = 1.0f;
        set->late.delay_to_crossover_frames = static_cast<float>(K_TAIL_DELAY);
        for (std::size_t section = 0; section < SushiDSP::room::FdnDesign::K_OUTPUT_SECTIONS; ++section)
        {
            set->late.tone_correction[section][0] = 1.0;
            set->late.coherence_b[section][0] = 1.0;
        }
        return set;
    }

    /// @brief Renders one block of a stage adopting tail_b_set, with a unit impulse in the B ring
    /// alone, and returns the sum of the right ear's tail over the block.
    float right_tail_sum(ListenerKind kind)
    {
        PairStage stage;
        stage.prepare(kind, K_RATE, K_ALIGNMENT_FRAMES, K_BLOCK);
        stage.adopt(*tail_b_set(kind));
        DelayRing ring;
        DelayRing tail_a;
        DelayRing tail_b;
        ring.prepare(K_RING_FRAMES, K_BLOCK);
        tail_a.prepare(K_RING_FRAMES, K_BLOCK);
        tail_b.prepare(K_RING_FRAMES, K_BLOCK);
        const std::vector<float> silence(K_BLOCK, 0.0f);
        std::vector<float> impulse(K_BLOCK, 0.0f);
        impulse[0] = 1.0f;
        ring.write(silence.data(), K_BLOCK);
        tail_a.write(silence.data(), K_BLOCK);
        tail_b.write(impulse.data(), K_BLOCK);

        stage.render(ring, tail_a, tail_b, K_BLOCK, FadeWindow{0, 0, 0});

        const float* const right = stage.tail(1);
        return std::accumulate(right, right + K_BLOCK, 0.0f);
    }

    /// @brief Returns an ears stage prepared at K_RATE.
    PairStage& prepared(PairStage& stage)
    {
        stage.prepare(ListenerKind::Ears, K_RATE, K_ALIGNMENT_FRAMES, K_BLOCK);
        return stage;
    }
} // namespace

TEST(Small_PairStage, IsNotLiveBeforeItAdoptsASet)
{
    PairStage stage;

    EXPECT_FALSE(prepared(stage).live());
}

TEST(Small_PairStage, IsLiveOnceItHasAdoptedASet)
{
    PairStage stage;
    prepared(stage).adopt(*three_tap_set(ListenerKind::Ears));

    EXPECT_TRUE(stage.live());
}

TEST(Small_PairStage, StaysLiveWhenItAdoptsASecondSet)
{
    PairStage stage;
    prepared(stage).adopt(*three_tap_set(ListenerKind::Ears));

    stage.adopt(*three_tap_set(ListenerKind::Ears));

    EXPECT_TRUE(stage.live());
}

TEST(Small_PairStage, IsNotLiveAfterResetOfAnAdoptedSet)
{
    PairStage stage;
    prepared(stage).adopt(*three_tap_set(ListenerKind::Ears));

    stage.reset();

    EXPECT_FALSE(stage.live());
}

TEST(Small_PairStage, TakeDirectImageReturnsTrueForAnEarsSet)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};

    EXPECT_TRUE(PairStage::take_direct_image(*three_tap_set(ListenerKind::Ears), reflections, direct));
}

TEST(Small_PairStage, TakeDirectImageCopiesTapZeroOfAnEarsSetIntoDirect)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};

    PairStage::take_direct_image(*three_tap_set(ListenerKind::Ears), reflections, direct);

    EXPECT_EQ(direct.gain, K_DIRECT_GAIN);
}

TEST(Small_PairStage, TakeDirectImageLeavesTheReflectionsOfAnEarsSetWithoutTapZero)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};

    PairStage::take_direct_image(*three_tap_set(ListenerKind::Ears), reflections, direct);

    EXPECT_EQ(reflections.count, 2u);
    EXPECT_EQ(reflections.taps[0].delay_frames, 10.0f);
    EXPECT_EQ(reflections.taps[1].delay_frames, 20.0f);
}

TEST(Small_PairStage, TakeDirectImageReturnsFalseForAMicrophoneSet)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};

    EXPECT_FALSE(PairStage::take_direct_image(*three_tap_set(ListenerKind::Microphone), reflections, direct));
}

TEST(Small_PairStage, TakeDirectImageLeavesDirectAsItWasForAMicrophoneSet)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};
    direct.gain = K_UNTOUCHED_GAIN;

    PairStage::take_direct_image(*three_tap_set(ListenerKind::Microphone), reflections, direct);

    EXPECT_EQ(direct.gain, K_UNTOUCHED_GAIN);
}

TEST(Small_PairStage, TakeDirectImageKeepsEveryTapOfAMicrophoneSetAsAReflection)
{
    EarlyTapSet reflections{};
    EarlyTap direct{};

    PairStage::take_direct_image(*three_tap_set(ListenerKind::Microphone), reflections, direct);

    EXPECT_EQ(reflections.count, 3u);
}

TEST(Small_PairStage, AnEarsStagePassesTheBSumToTheRightEar)
{
    EXPECT_EQ(right_tail_sum(ListenerKind::Ears), 1.0f);
}

TEST(Small_PairStage, AMicrophoneStageLeavesTheRightEarSilent)
{
    EXPECT_EQ(right_tail_sum(ListenerKind::Microphone), 0.0f);
}
