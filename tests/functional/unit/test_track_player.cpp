/**************************************************************************/
/* test_track_player.cpp                                                  */
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
 * @file test_track_player.cpp
 * @brief Tests the mixing, clip placement and speed of TrackPlayer.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/track_player.hpp"
#include "SushiDSP/capture/track_timeline.hpp"
#include "SushiDSP/capture/transport.hpp"
#include "SushiDSP/routing/pan_node.hpp"

namespace
{
    /// @brief Builds a mono take whose sample at frame n is 1 + n, so a mix can be checked
    /// sample for sample and any read offset shows up immediately.
    /// @param frames How many frames the take holds.
    /// @param scale A factor applied to every sample.
    /// @return The filled take.
    std::unique_ptr<SushiDSP::Take> ramp_take(std::size_t frames, float scale = 1.0f)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
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
        float* outputs[2] = {left.data(), right.data()};
        player.process(outputs, 2u, static_cast<std::uint32_t>(left.size()), transport);
    }

    /// @brief Returns the settled gains PanNode produces at a pan position.
    /// @param pan The pan position to measure.
    /// @param left Receives the left gain.
    /// @param right Receives the right gain.
    void pan_node_gains(float pan, float& left, float& right)
    {
        SushiDSP::PanNode node;
        node.prepare(SushiDSP::PrepareInfo{48000.0, 64, 1});
        node.set_parameter_for_control_thread("pan", pan);

        std::vector<float> input(64, 1.0f);
        std::vector<float> out_left(64, 0.0f);
        std::vector<float> out_right(64, 0.0f);
        const float* inputs[1] = {input.data()};
        float* outputs[2] = {out_left.data(), out_right.data()};
        SushiDSP::ProcessContext context{outputs, inputs, 64, 0.0};

        // PanNode snaps its smoother onto the target, so its output stops changing only once settled.
        constexpr int K_MAX_BLOCKS = 4000;
        float previous_left = 0.0f;
        float previous_right = 0.0f;
        for (int block = 0; block < K_MAX_BLOCKS; ++block)
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

    /// @brief Builds a mono take that is silent apart from one sample of 1.
    /// @param frames How many frames the take holds.
    /// @param at The frame carrying the impulse.
    /// @return The filled take.
    std::unique_ptr<SushiDSP::Take> impulse_take(std::size_t frames, std::size_t at)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        float* samples = take->mutable_channel(0);
        samples[at] = 1.0f;
        return take;
    }

    /// @brief Builds a mono take carrying one sine.
    /// @param frequency_hz Tone frequency in Hz.
    /// @param frames How many frames the take holds.
    /// @return The filled take.
    std::unique_ptr<SushiDSP::Take> sine_take(double frequency_hz, std::size_t frames)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] =
                static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * frequency_hz *
                                            static_cast<double>(frame) / 48000.0));
        }
        return take;
    }

    /// @brief Hands a take to a player's library and returns the pointer its clips borrow.
    /// @param player The player owning the library.
    /// @param take The take to hand over.
    /// @return The borrowed pointer.
    const SushiDSP::Take* lend(SushiDSP::TrackPlayer& player, std::unique_ptr<SushiDSP::Take> take)
    {
        return player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));
    }

    /// @brief Publishes one lane's clips.
    /// @param player The player to publish into.
    /// @param lane Lane index.
    /// @param clips The lane's clips.
    void place(SushiDSP::TrackPlayer& player, std::size_t lane,
               const std::vector<SushiDSP::Clip>& clips)
    {
        player.set_timeline(lane, std::unique_ptr<SushiDSP::TrackTimeline>(
                                      new SushiDSP::TrackTimeline(player.takes(), clips)));
    }

    /// @brief Counts sign changes in a run of samples, which is twice the cycles it holds.
    /// @param samples The run to count over.
    /// @param first The first sample to look at.
    /// @return How many sign changes the run holds.
    int zero_crossings(const std::vector<float>& samples, std::size_t first)
    {
        int crossings = 0;
        for (std::size_t frame = first; frame + 1 < samples.size(); ++frame)
        {
            if ((samples[frame] <= 0.0f) != (samples[frame + 1] <= 0.0f))
            {
                ++crossings;
            }
        }
        return crossings;
    }
} // namespace

TEST(Unit_Dsp, TrackPlayerUnityTrackReproducesItsTakeSampleForSample)
{
    SushiDSP::Transport transport;
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
    SushiDSP::Transport transport;
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
    SushiDSP::Transport transport;
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
    SushiDSP::Transport transport;
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

        SushiDSP::Transport transport;
        SushiDSP::TrackPlayer player;

        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, std::size_t{64});
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

        SushiDSP::Transport transport;
        SushiDSP::TrackPlayer player;
        player.set_take(0, ramp_take(64));
        player.set_pan(0, pan);
        player.set_gain(0, 0.5f);

        std::vector<float> mono(64, 0.0f);
        float* outputs[1] = {mono.data()};
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
    SushiDSP::Transport transport;
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
    SushiDSP::Transport transport;
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

TEST(Unit_Dsp, TrackPlayerPlaysAClipAtItsStartFrame)
{
    for (std::uint64_t start : {std::uint64_t{0}, std::uint64_t{12345}, std::uint64_t{1000000}})
    {
        SushiDSP::TrackPlayer player;
        SushiDSP::Transport transport;

        const SushiDSP::Take* take = lend(player, impulse_take(8u, 3u));
        place(player, 0, {SushiDSP::Clip{take, start, 0u, 8u}});
        player.set_pan(0, -1.0f);

        // The impulse sits at the clip's fourth frame. See tests/functional/unit/README.md.
        const std::uint64_t block = (start / 64u) * 64u;
        transport.seek(block);

        std::vector<float> left(64, 0.0f);
        std::vector<float> right(64, 0.0f);
        render(player, transport, left, right);

        const std::size_t expected = static_cast<std::size_t>(start + 3u - block);
        for (std::size_t frame = 0; frame < left.size(); ++frame)
        {
            EXPECT_FLOAT_EQ(left[frame], frame == expected ? 1.0f : 0.0f)
                << "at start " << start << ", frame " << frame;
        }
    }
}

TEST(Unit_Dsp, TrackPlayerPlaysTwoClipsOnOneLane)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    const SushiDSP::Take* take = lend(player, impulse_take(4u, 0u));
    place(player, 0, {SushiDSP::Clip{take, 0u, 0u, 4u}, SushiDSP::Clip{take, 20u, 0u, 4u}});
    player.set_pan(0, -1.0f);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    EXPECT_FLOAT_EQ(left[0], 1.0f);
    EXPECT_FLOAT_EQ(left[20], 1.0f);
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        if (frame != 0 && frame != 20)
        {
            EXPECT_FLOAT_EQ(left[frame], 0.0f) << "at frame " << frame;
        }
    }
}

TEST(Unit_Dsp, TrackPlayerIsSilentBetweenClips)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    auto source = std::make_unique<SushiDSP::Take>(1u, 48000.0, std::size_t{8});
    float* samples = source->mutable_channel(0);
    for (std::size_t frame = 0; frame < 8; ++frame)
    {
        samples[frame] = 1.0f;
    }

    const SushiDSP::Take* take = lend(player, std::move(source));
    place(player, 0, {SushiDSP::Clip{take, 0u, 0u, 8u}, SushiDSP::Clip{take, 40u, 0u, 8u}});
    player.set_pan(0, -1.0f);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    for (std::size_t frame = 0; frame < 8; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 1.0f) << "at frame " << frame;
    }
    for (std::size_t frame = 8; frame < 40; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f) << "at frame " << frame;
    }
    for (std::size_t frame = 40; frame < 48; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 1.0f) << "at frame " << frame;
    }
    for (std::size_t frame = 48; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 0.0f) << "at frame " << frame;
    }
}

TEST(Unit_Dsp, TrackPlayerFadesAClipEdge)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    auto source = std::make_unique<SushiDSP::Take>(1u, 48000.0, std::size_t{4096});
    float* samples = source->mutable_channel(0);
    for (std::size_t frame = 0; frame < 4096; ++frame)
    {
        samples[frame] = 1.0f;
    }

    // A clip cut inside its take carries the ramp. See tests/functional/unit/README.md.
    const SushiDSP::Take* take = lend(player, std::move(source));
    place(player, 0, {SushiDSP::Clip{take, 0u, 1000u, 2000u}});
    player.set_pan(0, -1.0f);

    std::vector<float> left(512, 0.0f);
    std::vector<float> right(512, 0.0f);
    render(player, transport, left, right);

    const float ramp = static_cast<float>(SushiDSP::TrackTimeline::K_FADE_FRAMES);
    for (std::size_t frame = 0; frame < SushiDSP::TrackTimeline::K_FADE_FRAMES; ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], static_cast<float>(frame) / ramp) << "at frame " << frame;
    }
    for (std::size_t frame = SushiDSP::TrackTimeline::K_FADE_FRAMES; frame < left.size(); ++frame)
    {
        EXPECT_FLOAT_EQ(left[frame], 1.0f) << "at frame " << frame;
    }
}

TEST(Unit_Dsp, TrackPlayerAtUnitySpeedReproducesItsTakeBitForBit)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    auto source = sine_take(997.0, 1024u);
    const std::vector<float> reference(source->channel(0), source->channel(0) + 256);

    player.set_take(0, std::move(source));
    player.set_pan(0, -1.0f);
    player.prepare(256u);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    render(player, transport, left, right);

    // The converter must be bypassed rather than run at a ratio of one, so the bits match.
    for (std::size_t frame = 0; frame < left.size(); ++frame)
    {
        EXPECT_EQ(std::memcmp(&left[frame], &reference[frame], sizeof(float)), 0)
            << "at frame " << frame;
    }
}

TEST(Unit_Dsp, TrackPlayerAtHalfSpeedDropsAnOctave)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    player.set_take(0, sine_take(880.0, 96000u));
    player.set_pan(0, -1.0f);
    player.prepare(256u);
    transport.set_speed(0.5);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    std::vector<float> rendered;
    for (int block = 0; block < 200; ++block)
    {
        render(player, transport, left, right);
        transport.advance(256);
        rendered.insert(rendered.end(), left.begin(), left.end());
    }

    // 24000 frames hold 220 cycles of 440 Hz. See tests/functional/unit/README.md.
    EXPECT_EQ(zero_crossings(rendered, rendered.size() - 24000u), 440);

    double peak = 0.0;
    for (std::size_t frame = rendered.size() - 24000u; frame < rendered.size(); ++frame)
    {
        peak = std::max(peak, std::fabs(static_cast<double>(rendered[frame])));
    }
    EXPECT_NEAR(peak, 1.0, 0.01);
}

TEST(Unit_Dsp, TrackPlayerReclaimsLanesBeforeCollectingTheLibrary)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    const SushiDSP::Take* take = lend(player, impulse_take(64u, 0u));
    place(player, 0, {SushiDSP::Clip{take, 0u, 0u, 64u}});

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    place(player, 0, {});
    player.reclaim();

    // The first timeline is alive, so its take is held. See tests/functional/unit/README.md.
    EXPECT_EQ(player.takes().size(), 1u);
    EXPECT_EQ(player.takes().reference_count(take), 1u);

    render(player, transport, left, right);
    player.reclaim();
    EXPECT_EQ(player.takes().size(), 0u);
}

TEST(Unit_Dsp, TrackPlayerFreesATakeOnceBothLanesUsingItAreCleared)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;

    const SushiDSP::Take* take = lend(player, impulse_take(64u, 0u));
    const std::vector<SushiDSP::Clip> clips{SushiDSP::Clip{take, 0u, 0u, 64u}};
    place(player, 0, clips);
    place(player, 1, clips);
    EXPECT_EQ(player.takes().reference_count(take), 2u);

    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    place(player, 0, {});
    render(player, transport, left, right);
    player.reclaim();
    EXPECT_EQ(player.takes().size(), 1u);
    EXPECT_EQ(player.takes().reference_count(take), 1u);

    place(player, 1, {});
    render(player, transport, left, right);
    render(player, transport, left, right);
    player.reclaim();
    EXPECT_EQ(player.takes().size(), 0u);
}
