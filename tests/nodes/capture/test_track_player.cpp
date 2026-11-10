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
 * @file test_track_player.cpp
 * @brief Tests the mixing, clip placement and speed of TrackPlayer.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>
#include <SushiDSP/nodes/capture/track_player.hpp>
#include <SushiDSP/nodes/capture/track_timeline.hpp>
#include <SushiDSP/nodes/capture/transport.hpp>
#include <SushiDSP/nodes/routing/pan_node.hpp>
#include <SushiDSP/nodes/capture/take.hpp>

#include "support/tolerance.hpp"

namespace
{
    constexpr double K_TWO_PI = 6.283185307179586;

    /// @brief The band within which the converter holds a 440 Hz tone's amplitude at half speed.
    /// It is characterized from the first version of the test and has no derivation yet.
    constexpr double K_CONVERTER_AMPLITUDE_BAND = 0.01;

    /// @brief Names a take to read and the frame to start reading at.
    struct UnityRow
    {
        const char* name;
        std::unique_ptr<SushiDSP::Take> (*make_take)();
        std::uint64_t start_frame;
    };

    /// @brief Names a pan position.
    struct PanRow
    {
        const char* name;
        float pan;
    };

    /// @brief Names a clip start frame.
    struct StartRow
    {
        const char* name;
        std::uint64_t start;
    };

    /// @brief Names how many lanes hold one take.
    struct LaneRow
    {
        const char* name;
        std::size_t lanes;
    };

    /// @brief Names a prepared block size and the forced-unity-speed count it must give.
    struct PreparedRow
    {
        const char* name;
        std::uint32_t prepared_frames;
        unsigned forced_unity_speed;
    };

    const PanRow K_PAN_ROWS[] = {
        {"PanMinusOne", -1.0f},      {"PanMinusThreeQuarters", -0.75f}, {"PanMinusHalf", -0.5f},
        {"PanMinusQuarter", -0.25f}, {"PanCentre", 0.0f},               {"PanQuarter", 0.25f},
        {"PanHalf", 0.5f},           {"PanThreeQuarters", 0.75f},       {"PanOne", 1.0f}};

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

    /// @brief Builds a mono take of ones.
    std::unique_ptr<SushiDSP::Take> ones_take(std::size_t frames)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        std::fill(take->mutable_channel(0), take->mutable_channel(0) + frames, 1.0f);
        return take;
    }

    /// @brief Builds a mono take that is silent apart from one sample of 1.
    std::unique_ptr<SushiDSP::Take> impulse_take(std::size_t frames, std::size_t at)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        take->mutable_channel(0)[at] = 1.0f;
        return take;
    }

    /// @brief Fills channel 0 of a take with a sine.
    void fill_sine(SushiDSP::Take& take, double frequency_hz, float amplitude)
    {
        float* samples = take.mutable_channel(0);
        for (std::size_t frame = 0; frame < take.frames(); ++frame)
        {
            samples[frame] = amplitude * static_cast<float>(std::sin(K_TWO_PI * frequency_hz *
                                                                     static_cast<double>(frame) / 48000.0));
        }
    }

    /// @brief Builds a mono take carrying one sine of amplitude 1.
    std::unique_ptr<SushiDSP::Take> sine_take(double frequency_hz, std::size_t frames)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        fill_sine(*take, frequency_hz, 1.0f);
        return take;
    }

    /// @brief Returns count ramp values starting at a take frame: scale * (1 + frame).
    std::vector<float> ramp_values(std::size_t first_frame, std::size_t count, float scale = 1.0f)
    {
        std::vector<float> values(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            values[i] = scale * (1.0f + static_cast<float>(first_frame + i));
        }
        return values;
    }

    /// @brief Joins runs of samples in order.
    std::vector<float> joined(const std::vector<std::vector<float>>& runs)
    {
        std::vector<float> all;
        for (const std::vector<float>& run : runs)
        {
            all.insert(all.end(), run.begin(), run.end());
        }
        return all;
    }

    /// @brief Returns count copies of one value.
    std::vector<float> repeated(float value, std::size_t count)
    {
        return std::vector<float>(count, value);
    }

    /// @brief Returns count zeros with ones at the given frames.
    std::vector<float> zeros_with_ones_at(std::size_t count, const std::vector<std::size_t>& frames)
    {
        std::vector<float> values(count, 0.0f);
        for (std::size_t frame : frames)
        {
            values[frame] = 1.0f;
        }
        return values;
    }

    /// @brief Mixes one block into a freshly zeroed stereo pair.
    void render(SushiDSP::TrackPlayer& player, SushiDSP::Transport& transport,
                std::vector<float>& left, std::vector<float>& right)
    {
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        float* outputs[2] = {left.data(), right.data()};
        player.process(outputs, 2u, static_cast<std::uint32_t>(left.size()), transport);
    }

    /// @brief Mixes a block of frames and returns the left channel.
    std::vector<float> render_left(SushiDSP::TrackPlayer& player, SushiDSP::Transport& transport,
                                   std::size_t frames)
    {
        std::vector<float> left(frames, 0.0f);
        std::vector<float> right(frames, 0.0f);
        render(player, transport, left, right);
        return left;
    }

    /// @brief Mixes blocks one after another, advancing the transport between them, and returns
    /// the left channel of all of them in order.
    std::vector<float> render_advancing(SushiDSP::TrackPlayer& player,
                                        SushiDSP::Transport& transport, int blocks,
                                        std::uint32_t block_frames)
    {
        std::vector<float> left(block_frames, 0.0f);
        std::vector<float> right(block_frames, 0.0f);
        std::vector<float> rendered;
        for (int block = 0; block < blocks; ++block)
        {
            render(player, transport, left, right);
            transport.advance(block_frames);
            rendered.insert(rendered.end(), left.begin(), left.end());
        }
        return rendered;
    }

    /// @brief Returns the settled gains PanNode produces at a pan position.
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

    /// @brief Counts sign changes in a run of samples, which is twice the cycles it holds.
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

    /// @brief Returns the largest magnitude from a frame to the end.
    double peak_from(const std::vector<float>& samples, std::size_t first)
    {
        double peak = 0.0;
        for (std::size_t frame = first; frame < samples.size(); ++frame)
        {
            peak = std::max(peak, std::fabs(static_cast<double>(samples[frame])));
        }
        return peak;
    }

    /// @brief Hands a take to a player's library and returns the pointer its clips borrow.
    const SushiDSP::Take* lend(SushiDSP::TrackPlayer& player, std::unique_ptr<SushiDSP::Take> take)
    {
        return player.takes().insert(std::unique_ptr<const SushiDSP::Take>(std::move(take)));
    }

    /// @brief Publishes one lane's clips.
    void place(SushiDSP::TrackPlayer& player, std::size_t lane,
               const std::vector<SushiDSP::Clip>& clips)
    {
        player.set_timeline(lane, std::unique_ptr<SushiDSP::TrackTimeline>(
                                      new SushiDSP::TrackTimeline(player.takes(), clips)));
    }

    /// @brief Expects the samples to equal the expected values with no rounding at all.
    void expect_samples_equal(const std::vector<float>& actual, const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t frame = 0; frame < expected.size(); ++frame)
        {
            EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(actual[frame], expected[frame], 0.0)))
                << "at frame " << frame;
        }
    }

    /// @brief Expects the samples to equal the expected values bit for bit.
    void expect_bits_equal(const std::vector<float>& actual, const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t frame = 0; frame < expected.size(); ++frame)
        {
            EXPECT_EQ(std::memcmp(&actual[frame], &expected[frame], sizeof(float)), 0)
                << "at frame " << frame;
        }
    }

    /// @brief Expects the samples to follow the expected values to the rounding of two float
    /// operations: the multiply by a gain and the add into the mix.
    void expect_samples_near(const std::vector<float>& actual, const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t frame = 0; frame < expected.size(); ++frame)
        {
            EXPECT_TRUE(TestSupport::is_within(
                TestSupport::measure_relative_error<float>(actual[frame], expected[frame], 2)))
                << "at frame " << frame;
        }
    }

    void expect_finite(const std::vector<float>& samples)
    {
        for (std::size_t frame = 0; frame < samples.size(); ++frame)
        {
            EXPECT_TRUE(std::isfinite(samples[frame])) << "at frame " << frame;
        }
    }

    /// @brief Clears each lane in turn, lets the audio thread pass the swap, reclaims, and expects
    /// the take to be held by exactly the lanes not yet cleared.
    void expect_take_held_by_the_lanes_left(SushiDSP::TrackPlayer& player,
                                            SushiDSP::Transport& transport,
                                            const SushiDSP::Take* take, std::size_t lanes)
    {
        std::vector<float> left(64, 0.0f);
        std::vector<float> right(64, 0.0f);
        for (std::size_t cleared = 0; cleared < lanes; ++cleared)
        {
            place(player, cleared, {});
            render(player, transport, left, right);
            render(player, transport, left, right);
            player.reclaim();
            const std::size_t holders = lanes - cleared - 1u;
            EXPECT_EQ(player.takes().size(), holders == 0u ? 0u : 1u) << "after lane " << cleared;
            if (holders != 0u)
            {
                EXPECT_EQ(player.takes().reference_count(take), holders) << "after lane " << cleared;
            }
        }
    }

    /// @brief Fills a player with eight lanes of sixteen clips each, over mono and stereo takes.
    void fill_every_lane(SushiDSP::TrackPlayer& player)
    {
        for (std::size_t track = 0; track < SushiDSP::TrackPlayer::K_MAX_TRACKS; ++track)
        {
            auto take = std::make_unique<SushiDSP::Take>(track % 2 == 0 ? 1u : 2u, 48000.0, 4096u);
            fill_sine(*take, 110.0, 0.25f);
            const SushiDSP::Take* borrowed = lend(player, std::move(take));

            std::vector<SushiDSP::Clip> clips;
            for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::K_MAX_CLIPS; ++clip)
            {
                clips.push_back(SushiDSP::Clip{borrowed, clip * 200u, 64u, 150u});
            }
            place(player, track, clips);
            player.set_pan(track, -1.0f + 0.25f * static_cast<float>(track));
            player.set_gain(track, 0.5f);
        }
    }

    /// @brief Builds timelines over one take, each a little later than the one before.
    std::vector<std::unique_ptr<SushiDSP::TrackTimeline>>
    shifted_timelines(SushiDSP::TrackPlayer& player, const SushiDSP::Take* take, int count)
    {
        std::vector<std::unique_ptr<SushiDSP::TrackTimeline>> prepared;
        for (int swap = 0; swap < count; ++swap)
        {
            std::vector<SushiDSP::Clip> clips;
            for (std::size_t clip = 0; clip < SushiDSP::TrackTimeline::K_MAX_CLIPS; ++clip)
            {
                clips.push_back(SushiDSP::Clip{take, static_cast<std::uint64_t>(swap) + clip * 300u,
                                               32u, 250u});
            }
            prepared.push_back(std::unique_ptr<SushiDSP::TrackTimeline>(
                new SushiDSP::TrackTimeline(player.takes(), clips)));
        }
        return prepared;
    }

    /// @brief Mixes blocks at each of two speeds with the trap armed around all of them.
    void mix_trapped_at_two_speeds(SushiDSP::TrackPlayer& player, SushiDSP::Transport& transport,
                                   std::vector<float>& left, std::vector<float>& right,
                                   SushiDSP::test::AllocationTrap& trap)
    {
        float* outputs[2] = {left.data(), right.data()};
        trap.arm();
        for (double speed : {1.0, 0.5})
        {
            transport.set_speed(speed);
            for (int block = 0; block < 200; ++block)
            {
                std::fill(left.begin(), left.end(), 0.0f);
                std::fill(right.begin(), right.end(), 0.0f);
                player.process(outputs, 2u, 256u, transport);
                transport.advance(256);
            }
        }
        trap.disarm();
    }

    /// @brief Swaps in each prepared timeline and mixes one trapped block after each swap.
    void swap_and_mix_trapped(SushiDSP::TrackPlayer& player, SushiDSP::Transport& transport,
                              std::vector<std::unique_ptr<SushiDSP::TrackTimeline>>& prepared,
                              std::vector<float>& left, std::vector<float>& right,
                              SushiDSP::test::AllocationTrap& trap)
    {
        float* outputs[2] = {left.data(), right.data()};
        for (std::unique_ptr<SushiDSP::TrackTimeline>& timeline : prepared)
        {
            player.set_timeline(0, std::move(timeline));

            trap.arm();
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            player.process(outputs, 2u, 256u, transport);
            trap.disarm();

            transport.advance(256);
        }
    }
} // namespace

class Small_TrackPlayerUnity : public ::testing::TestWithParam<UnityRow>
{
};

TEST_P(Small_TrackPlayerUnity, TrackPlayerAtUnitySpeedReproducesItsTakeBitForBit)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    auto source = GetParam().make_take();
    const float* head = source->channel(0) + GetParam().start_frame;
    const std::vector<float> reference(head, head + 64);
    player.set_take(0, std::move(source));
    player.set_pan(0, -1.0f);
    player.prepare(256u);
    transport.seek(GetParam().start_frame);

    // The converter must be bypassed rather than run at a ratio of one, so the bits match.
    const std::vector<float> left = render_left(player, transport, 64);

    expect_bits_equal(left, reference);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_TrackPlayerUnity,
    ::testing::Values(UnityRow{"RampFromTheStart", [] { return ramp_take(256); }, 0u},
                      UnityRow{"RampAfterOneBlock", [] { return ramp_take(256); }, 64u},
                      UnityRow{"SineFromTheStart", [] { return sine_take(997.0, 1024u); }, 0u}),
    [](const ::testing::TestParamInfo<UnityRow>& info) { return std::string(info.param.name); });

TEST(Small_TrackPlayer, TrackPlayerMutedTrackContributesNothing)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_mute(0, true);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);

    render(player, transport, left, right);

    expect_samples_equal(left, repeated(0.0f, 64));
    expect_samples_equal(right, repeated(0.0f, 64));
}

TEST(Small_TrackPlayer, TrackPlayerSoloSilencesTheOtherTracks)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_take(1, ramp_take(256, 10.0f));
    player.set_pan(0, -1.0f);
    player.set_pan(1, -1.0f);
    player.set_solo(1, true);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, ramp_values(0, 64, 10.0f));
}

TEST(Small_TrackPlayer, TrackPlayerTwoTracksSum)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_take(1, ramp_take(256, 3.0f));
    player.set_pan(0, -1.0f);
    player.set_pan(1, -1.0f);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, ramp_values(0, 64, 4.0f));
}

class Small_TrackPlayerPan : public ::testing::TestWithParam<PanRow>
{
};

TEST_P(Small_TrackPlayerPan, TrackPlayerPanLawMatchesPanNode)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ones_take(64));
    player.set_pan(0, GetParam().pan);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    float expected_left = 0.0f;
    float expected_right = 0.0f;
    pan_node_gains(GetParam().pan, expected_left, expected_right);

    render(player, transport, left, right);

    // Both gains are at most one, so the two evaluations of the law agree to a few float roundings.
    const double bound = 8.0 * TestSupport::unit_roundoff<float>();
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(left.back(), expected_left, bound)));
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(right.back(), expected_right, bound)));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TrackPlayerPan, ::testing::ValuesIn(K_PAN_ROWS),
                         [](const ::testing::TestParamInfo<PanRow>& info)
                         { return std::string(info.param.name); });

class Small_TrackPlayerMono : public ::testing::TestWithParam<PanRow>
{
};

TEST_P(Small_TrackPlayerMono, TrackPlayerMonoOutputIgnoresPan)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(64));
    player.set_pan(0, GetParam().pan);
    player.set_gain(0, 0.5f);
    std::vector<float> mono(64, 0.0f);
    float* outputs[1] = {mono.data()};

    player.process(outputs, 1u, static_cast<std::uint32_t>(mono.size()), transport);

    expect_samples_equal(mono, ramp_values(0, 64, 0.5f));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TrackPlayerMono, ::testing::ValuesIn(K_PAN_ROWS),
                         [](const ::testing::TestParamInfo<PanRow>& info)
                         { return std::string(info.param.name); });

TEST(Small_TrackPlayer, TrackPlayerPlayingPastTheEndYieldsSilence)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(100));
    player.set_pan(0, -1.0f);
    transport.seek(80);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, joined({ramp_values(80, 20), repeated(0.0f, 44)}));
}

TEST(Small_TrackPlayer, TrackPlayerSeekingPastTheEndYieldsSilence)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(100));
    player.set_pan(0, -1.0f);
    transport.seek(1000);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, repeated(0.0f, 64));
}

TEST(Small_TrackPlayer, TrackPlayerLoopWrapsAtExactlyTheLoopEnd)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_pan(0, -1.0f);
    transport.set_loop(10, 50);
    transport.seek(30);

    const std::vector<float> left = render_left(player, transport, 64);

    // Frames 30 to 49, then two full passes of the loop from frame 10 for 40 frames and 4 more.
    expect_samples_equal(left, joined({ramp_values(30, 20), ramp_values(10, 40), ramp_values(10, 4)}));
}

class Small_TrackPlayerClipStart : public ::testing::TestWithParam<StartRow>
{
};

TEST_P(Small_TrackPlayerClipStart, TrackPlayerPlaysAClipAtItsStartFrame)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    const SushiDSP::Take* take = lend(player, impulse_take(8u, 3u));
    place(player, 0, {SushiDSP::Clip{take, GetParam().start, 0u, 8u}});
    player.set_pan(0, -1.0f);
    // The impulse sits at the clip's fourth frame. See tests/nodes/capture/README.md.
    const std::uint64_t block = (GetParam().start / 64u) * 64u;
    transport.seek(block);

    const std::vector<float> left = render_left(player, transport, 64);

    const auto expected = static_cast<std::size_t>(GetParam().start + 3u - block);
    expect_samples_equal(left, zeros_with_ones_at(64, {expected}));
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TrackPlayerClipStart,
                         ::testing::Values(StartRow{"StartZero", 0u}, StartRow{"Start12345", 12345u},
                                           StartRow{"Start1000000", 1000000u}),
                         [](const ::testing::TestParamInfo<StartRow>& info)
                         { return std::string(info.param.name); });

TEST(Small_TrackPlayer, TrackPlayerPlaysTwoClipsOnOneLane)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    const SushiDSP::Take* take = lend(player, impulse_take(4u, 0u));
    place(player, 0, {SushiDSP::Clip{take, 0u, 0u, 4u}, SushiDSP::Clip{take, 20u, 0u, 4u}});
    player.set_pan(0, -1.0f);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, zeros_with_ones_at(64, {0u, 20u}));
}

TEST(Small_TrackPlayer, TrackPlayerIsSilentBetweenClips)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    const SushiDSP::Take* take = lend(player, ones_take(8));
    place(player, 0, {SushiDSP::Clip{take, 0u, 0u, 8u}, SushiDSP::Clip{take, 40u, 0u, 8u}});
    player.set_pan(0, -1.0f);

    const std::vector<float> left = render_left(player, transport, 64);

    expect_samples_equal(left, joined({repeated(1.0f, 8), repeated(0.0f, 32), repeated(1.0f, 8),
                                       repeated(0.0f, 16)}));
}

TEST(Small_TrackPlayer, TrackPlayerFadesAClipEdge)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    // A clip cut inside its take carries the ramp. See tests/nodes/capture/README.md.
    const SushiDSP::Take* take = lend(player, ones_take(4096));
    place(player, 0, {SushiDSP::Clip{take, 0u, 1000u, 2000u}});
    player.set_pan(0, -1.0f);

    const std::vector<float> left = render_left(player, transport, 512);

    const std::size_t ramp_frames = SushiDSP::TrackTimeline::K_FADE_FRAMES;
    std::vector<float> expected(512, 1.0f);
    for (std::size_t frame = 0; frame < ramp_frames; ++frame)
    {
        expected[frame] = static_cast<float>(frame) / static_cast<float>(ramp_frames);
    }
    expect_samples_near(left, expected);
}

TEST(Small_TrackPlayer, TrackPlayerAtHalfSpeedDropsAnOctave)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    player.set_take(0, sine_take(880.0, 96000u));
    player.set_pan(0, -1.0f);
    player.prepare(256u);
    transport.set_speed(0.5);

    const std::vector<float> rendered = render_advancing(player, transport, 200, 256u);

    // 24000 frames hold 220 cycles of 440 Hz. See tests/nodes/capture/README.md.
    EXPECT_EQ(zero_crossings(rendered, rendered.size() - 24000u), 440);
}

TEST(Small_TrackPlayer, TrackPlayerAtHalfSpeedKeepsTheToneAmplitude)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    player.set_take(0, sine_take(880.0, 96000u));
    player.set_pan(0, -1.0f);
    player.prepare(256u);
    transport.set_speed(0.5);

    const std::vector<float> rendered = render_advancing(player, transport, 200, 256u);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(
        peak_from(rendered, rendered.size() - 24000u), 1.0, K_CONVERTER_AMPLITUDE_BAND, 1.0)));
}

TEST(Small_TrackPlayer, TrackPlayerHoldsATakeOfAClearedLaneUntilTheAudioThreadHasPassedIt)
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

    // The first timeline is alive, so its take is held. See tests/nodes/capture/README.md.
    EXPECT_EQ(player.takes().size(), 1u);
    EXPECT_EQ(player.takes().reference_count(take), 1u);
}

class Small_TrackPlayerReclaim : public ::testing::TestWithParam<LaneRow>
{
};

TEST_P(Small_TrackPlayerReclaim, TrackPlayerFreesATakeOnceTheLanesUsingItAreCleared)
{
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    const SushiDSP::Take* take = lend(player, impulse_take(64u, 0u));
    const std::vector<SushiDSP::Clip> clips{SushiDSP::Clip{take, 0u, 0u, 64u}};
    for (std::size_t lane = 0; lane < GetParam().lanes; ++lane)
    {
        place(player, lane, clips);
    }
    ASSERT_EQ(player.takes().reference_count(take), GetParam().lanes);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    render(player, transport, left, right);

    expect_take_held_by_the_lanes_left(player, transport, take, GetParam().lanes);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TrackPlayerReclaim,
                         ::testing::Values(LaneRow{"OneLane", 1u}, LaneRow{"TwoLanes", 2u}),
                         [](const ::testing::TestParamInfo<LaneRow>& info)
                         { return std::string(info.param.name); });

TEST(Small_TrackPlayer, TrackPlayerRendersOneLaneAloneBeforeItsFader)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    player.set_take(1, ramp_take(256, 100.0f));
    // The lane's own fader says silence; a lane rendered alone does not read it.
    player.set_gain(1, 0.0f);
    player.set_pan(1, -1.0f);
    player.set_mute(1, true);
    player.set_solo(0, true);
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    float* outputs[2] = {left.data(), right.data()};

    player.process_lane(1, outputs, 2u, 64u, transport);

    // A mono take lands on both channels at unity, and lane 0 adds nothing.
    expect_samples_equal(left, ramp_values(0, 64, 100.0f));
    expect_samples_equal(right, ramp_values(0, 64, 100.0f));
}

TEST(Small_TrackPlayer, TrackPlayerRendersNothingForALaneBeyondTheLastTrack)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));
    std::vector<float> left(64, 0.0f);
    std::vector<float> right(64, 0.0f);
    float* outputs[2] = {left.data(), right.data()};

    player.process_lane(SushiDSP::TrackPlayer::K_MAX_TRACKS, outputs, 2u, 64u, transport);

    expect_samples_equal(left, repeated(0.0f, 64));
}

TEST(Medium_TrackPlayer, ProcessPerformsNoHeapAllocation)
{
    // Timelines and converters exist before the trap opens. See tests/nodes/capture/README.md.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    fill_every_lane(player);
    player.set_mute(1, true);
    player.prepare(256u);
    transport.set_loop(0, 3000);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    SushiDSP::test::AllocationTrap trap;

    mix_trapped_at_two_speeds(player, transport, left, right, trap);

    EXPECT_EQ(trap.count(), 0);
    expect_finite(left);
    expect_finite(right);
}

TEST(Medium_TrackPlayer, TimelineSwapPerformsNoHeapAllocationOnTheAudioThread)
{
    // The trap covers the pick-up of a changed lane. See tests/nodes/capture/README.md.
    SushiDSP::TrackPlayer player;
    SushiDSP::Transport transport;
    auto take = std::make_unique<SushiDSP::Take>(2u, 48000.0, 4096u);
    fill_sine(*take, 110.0, 0.25f);
    const SushiDSP::Take* borrowed = lend(player, std::move(take));
    std::vector<std::unique_ptr<SushiDSP::TrackTimeline>> prepared =
        shifted_timelines(player, borrowed, 64);
    player.prepare(256u);
    player.set_pan(0, -0.5f);
    transport.set_speed(0.75);
    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    SushiDSP::test::AllocationTrap trap;

    swap_and_mix_trapped(player, transport, prepared, left, right, trap);

    EXPECT_EQ(trap.count(), 0);
    expect_finite(left);
    expect_finite(right);
}

TEST(Small_TrackPlayerCounters, ACallWithNoOutputBufferIsCountedAndWritesNothing)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(256));

    player.process(nullptr, 2u, 64u, transport);
    float* outputs[1] = {nullptr};
    player.process(outputs, 0u, 64u, transport);

    EXPECT_EQ(player.counters().empty_call.value(), 2U);
    EXPECT_EQ(player.counters().forced_unity_speed.value(), 0U);
}

class Small_TrackPlayerPrepared : public ::testing::TestWithParam<PreparedRow>
{
};

TEST_P(Small_TrackPlayerPrepared, ABlockIsCountedAsForcedUnitySpeedOnlyWhenLongerThanThePreparedSize)
{
    SushiDSP::Transport transport;
    SushiDSP::TrackPlayer player;
    player.set_take(0, ramp_take(1024));
    player.prepare(GetParam().prepared_frames);
    transport.set_speed(0.5);
    std::vector<float> left(128, 0.0f);
    std::vector<float> right(128, 0.0f);

    render(player, transport, left, right);

    EXPECT_EQ(player.counters().forced_unity_speed.value(), GetParam().forced_unity_speed);
    EXPECT_EQ(player.counters().empty_call.value(), 0U);
}

INSTANTIATE_TEST_SUITE_P(Rows, Small_TrackPlayerPrepared,
                         ::testing::Values(PreparedRow{"LongerThanPrepared", 64u, 1u},
                                           PreparedRow{"InsidePrepared", 256u, 0u}),
                         [](const ::testing::TestParamInfo<PreparedRow>& info)
                         { return std::string(info.param.name); });
