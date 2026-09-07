/**************************************************************************/
/* test_track_player.cpp                                                  */
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

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/TrackPlayer.hpp"
#include "SushiDSP/capture/Transport.hpp"
#include "SushiDSP/routing/PanNode.hpp"

namespace
{
    /// @brief Builds a mono take whose sample at frame n is 1 + n, so a mix can be checked
    /// sample for sample and any read offset shows up immediately.
    /// @param frames How many frames the take holds.
    /// @param scale A factor applied to every sample.
    /// @return The filled take.
    std::unique_ptr<SushiDSP::Take> ramp_take(std::size_t frames, float scale = 1.0f)
    {
        auto   take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = scale * (1.0f + static_cast<float>(frame));
        }
        return take;
    }

    /// @brief Mixes one block into a freshly zeroed stereo pair.
    /// @param player The player under test.
    /// @param transport The clock the player reads.
    /// @param left Receives the left channel.
    /// @param right Receives the right channel.
    void render(SushiDSP::TrackPlayer& player, SushiDSP::Transport& transport,
                std::vector<float>& left, std::vector<float>& right)
    {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        float* outputs[2] = { left.data(), right.data() };
        player.process(outputs, 2u, static_cast<std::uint32_t>(left.size()), transport);
    }

    /// @brief Returns the settled gains PanNode produces at a pan position.
    /// @param pan The pan position to measure.
    /// @param left Receives the left gain.
    /// @param right Receives the right gain.
    void pan_node_gains(float pan, float& left, float& right)
    {
        SushiDSP::PanNode node;
        node.prepare(SushiDSP::PrepareInfo{ 48000.0, 64, 1 });
        node.set_parameter("pan", pan);

        std::vector<float> input(64, 1.0f);
        std::vector<float> out_left(64, 0.0f);
        std::vector<float> out_right(64, 0.0f);
        const float*             inputs[1] = { input.data() };
        float*                   outputs[2] = { out_left.data(), out_right.data() };
        SushiDSP::ProcessContext context{ outputs, inputs, 64, 0.0 };

        // PanNode snaps its smoother onto the target, so its output stops changing only once settled.
        constexpr int kMaxBlocks = 4000;
        float         previous_left = 0.0f;
        float         previous_right = 0.0f;
        for (int block = 0; block < kMaxBlocks; ++block)
        {
            node.process(context);
            left = out_left.back();
            right = out_right.back();
            if (block > 0 && left == previous_left && right == previous_right)
            {
                break;
            }
            previous_left = left;
            previous_right = right;
        }
    }

    /// @brief Converts a linear gain to decibels, floored so a hard null stays finite.
    /// @param gain The linear gain.
    /// @return The gain in decibels.
    float to_db(float gain)
    {
        return 20.0f * std::log10(std::fabs(gain) + 1e-20f);
    }
}

TEST(Unit_Dsp, TrackPlayerUnityTrackReproducesItsTakeSampleForSample)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_pan(0, -1.0f);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 1.0f + static_cast<float>(frame));
    }

    transport.advance(64);
    render(player, transport, left, right);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 65.0f + static_cast<float>(frame));
    }
}

TEST(Unit_Dsp, TrackPlayerMutedTrackContributesNothing)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_mute(0, true);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f);
        EXPECT_FLOAT_EQ(right[frame], 0.0f);
    }
}

TEST(Unit_Dsp, TrackPlayerSoloSilencesTheOtherTracks)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_take(1, ramp_take(256, 10.0f));
    player.set_pan(0, -1.0f);
    player.set_pan(1, -1.0f);
    player.set_solo(1, true);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 10.0f * (1.0f + static_cast<float>(frame)));
    }
}

TEST(Unit_Dsp, TrackPlayerTwoTracksSum)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_take(1, ramp_take(256, 3.0f));
    player.set_pan(0, -1.0f);
    player.set_pan(1, -1.0f);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 4.0f * (1.0f + static_cast<float>(frame)));
    }
}

TEST(Unit_Dsp, TrackPlayerPanLawMatchesPanNodeWithinAHundredthOfADecibel)
{
    for (int step = 0; step < 9; ++step)
    {
        const float pan = -1.0f + 0.25f * static_cast<float>(step);

        SushiDSP::Transport   transport;
        SushiDSP::TrackPlayer player;

        auto   take = std::make_unique<SushiDSP::Take>(1u, 48000.0, std::size_t{ 64 });
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < 64; ++frame)
        {
            samples[frame] = 1.0f;
        }
        player.set_take(0, std::move(take));
        player.set_pan(0, pan);

        std::vector<float> left(64, 0.0f);
        std::vector<float> right(64, 0.0f);
        render(player, transport, left, right);

        float expected_left = 0.0f;
        float expected_right = 0.0f;
        pan_node_gains(pan, expected_left, expected_right);

        EXPECT_NEAR(to_db(left.back()), to_db(expected_left), 0.01f);
        EXPECT_NEAR(to_db(right.back()), to_db(expected_right), 0.01f);
    }
}

TEST(Unit_Dsp, TrackPlayerMonoOutputIgnoresPan)
{
    for (int step = 0; step < 9; ++step)
    {
        const float pan = -1.0f + 0.25f * static_cast<float>(step);

        SushiDSP::Transport   transport;
        SushiDSP::TrackPlayer player;
        player.set_take(0, ramp_take(64));
        player.set_pan(0, pan);
        player.set_gain(0, 0.5f);

        std::vector<float> mono(64, 0.0f);
        float*             outputs[1] = { mono.data() };
        player.process(outputs, 1u, static_cast<std::uint32_t>(mono.size()), transport);

        for (std::size_t frame = 0; frame < mono.size(); ++frame)
        {
            EXPECT_FLOAT_EQ(mono[frame], 0.5f * (1.0f + static_cast<float>(frame)))
                << "at pan " << pan << ", frame " << frame;
        }
    }
}

TEST(Unit_Dsp, TrackPlayerPlayingPastTheEndYieldsSilence)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(100));
    player.set_pan(0, -1.0f);
    transport.seek(80);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < 20; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 81.0f + static_cast<float>(frame));
    }
    for (std::size_t frame = 20; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f);
    }

    transport.seek(1000);
    render(player, transport, left, right);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f);
    }
}

TEST(Unit_Dsp, TrackPlayerLoopWrapsAtExactlyTheLoopEnd)
{
    SushiDSP::Transport   transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_pan(0, -1.0f);
    transport.set_loop(10, 50);
    transport.seek(30);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < 20; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 31.0f + static_cast<float>(frame));
    }
    for (std::size_t frame = 20; frame < 60; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 11.0f + static_cast<float>(frame - 20));
    }
    for (std::size_t frame = 60; frame < 64; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 11.0f + static_cast<float>(frame - 60));
    }
}

TEST(Unit_Dsp, TransportAdvanceWrapsAtTheLoopEnd)
{
    SushiDSP::Transport transport;
    transport.set_loop(10, 50);
    transport.seek(30);

    transport.advance(20);
    EXPECT_EQ(transport.position_frames(), 10u);

    transport.advance(45);
    EXPECT_EQ(transport.position_frames(), 15u);

    transport.clear_loop();
    EXPECT_FALSE(transport.has_loop());
    transport.advance(100);
    EXPECT_EQ(transport.position_frames(), 115u);
}

TEST(Unit_Dsp, TransportStateAndPositionRoundTrip)
{
    SushiDSP::Transport transport;
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Stopped);
    EXPECT_EQ(transport.position_frames(), 0u);
    EXPECT_FALSE(transport.has_loop());

    transport.play();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Playing);
    transport.record();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Recording);
    transport.stop();
    EXPECT_EQ(transport.state(), SushiDSP::TransportState::Stopped);

    transport.seek(4096);
    EXPECT_EQ(transport.position_frames(), 4096u);
    transport.advance(128);
    EXPECT_EQ(transport.position_frames(), 4224u);

    transport.set_loop(100, 200);
    EXPECT_TRUE(transport.has_loop());
    EXPECT_EQ(transport.loop_begin_frames(), 100u);
    EXPECT_EQ(transport.loop_end_frames(), 200u);

    transport.set_loop(200, 200);
    EXPECT_FALSE(transport.has_loop());

    transport.set_loop(0, 0x1'0000'0000ull);
    EXPECT_FALSE(transport.has_loop());
    EXPECT_EQ(transport.loop_begin_frames(), 0u);
    EXPECT_EQ(transport.loop_end_frames(), 0u);

    transport.set_loop(1, 0x1'0000'0000ull);
    EXPECT_TRUE(transport.has_loop());
    EXPECT_EQ(transport.loop_begin_frames(), 1u);
    EXPECT_EQ(transport.loop_end_frames(), 0x1'0000'0000ull);
}
