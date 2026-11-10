/**************************************************************************/
/* test_session_renderer.cpp                                              */
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
 * @file test_session_renderer.cpp
 * @brief Tests the block renderer's counters, its profiler zones and its one lane per track.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/profile_run.hpp>
#include <SushiDSP/foundation/profile/sushidsp_profile_names.hpp>

#include "foundation/profile/recording_build_test.hpp"
#include "session.hpp"
#include "session_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SushiDSP/nodes/capture/take.hpp>
#include "support/rig_products.hpp"
#include "support/tolerance.hpp"

namespace
{
    /// @brief Holds the block every case renders in.
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds what every session of the cases is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, K_BLOCK, 2};

    /// @brief Holds the warm-up periods of the profiled cases.
    constexpr std::uint32_t K_WARMUP = 2;

    /// @brief Holds the active periods of the profiled cases.
    constexpr std::uint32_t K_ACTIVE = 4;

    /// @brief Holds two output channels and the silent live input a render reads.
    struct Buffers
    {
            std::vector<float> left;  ///< Holds the left output.
            std::vector<float> right; ///< Holds the right output.
            std::vector<float> live;  ///< Holds the live input, both channels.

            /// @brief Sizes every buffer to @p frames.
            explicit Buffers(std::uint32_t frames)
                : left(frames, 0.5f), right(frames, 0.5f), live(frames, 0.0f)
            {

            }
    };
} // namespace

namespace
{
    /// @brief Holds a block length and what the renderer counts for it.
    struct BlockRow
    {
        std::uint32_t frames;
        std::uint64_t cuts;
        std::uint64_t peak;
    };

    class Small_SessionRendererBlock : public ::testing::TestWithParam<BlockRow>
    {
    };

    /// @brief Renders the eight rolling tracks of a full session under a profiled run.
    /// @return The run the profiler recorded.
    SushiDSP::profile::ProfiledRun profile_eight_rolling_tracks()
    {
        Session session(K_INFO);
        std::string error;
        while (session.track_count() < Session::K_MAX_TRACKS)
            session.add_track(error);
        session.timeline().transport().play();
        SessionRenderer renderer;
        renderer.prepare(K_BLOCK);
        Buffers buffers(K_BLOCK);
        float* outputs[2] = {buffers.left.data(), buffers.right.data()};

        return SushiDSP::profile::run_profiled(SushiDSP::profile::names::K_BENCH_TIMELINE, K_WARMUP, K_ACTIVE, 0U,
                                               [&]
                                               {
                                                   renderer.render(session, nullptr, outputs, K_BLOCK);
                                               });
    }

    /// @brief Returns the lane names the session writes, "track.0" up to the last track.
    std::vector<std::string> track_lane_names()
    {
        std::vector<std::string> names;
        for (std::size_t track = 0; track < Session::K_MAX_TRACKS; ++track)
            names.push_back("track." + std::to_string(track));
        std::sort(names.begin(), names.end());
        return names;
    }
} // namespace

TEST_P(Small_SessionRendererBlock, ABlockLongerThanThePreparedSizeIsCutAndCounted)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(GetParam().frames);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    renderer.render(session, nullptr, outputs, GetParam().frames);

    EXPECT_EQ(renderer.counters().cut_block.value(), GetParam().cuts);
    EXPECT_EQ(renderer.counters().cut_block.peak(), GetParam().peak);
}

INSTANTIATE_TEST_SUITE_P(Blocks, Small_SessionRendererBlock,
                         ::testing::Values(BlockRow{2 * K_BLOCK, 1U, 2U * K_BLOCK}, BlockRow{K_BLOCK, 0U, 0U}));

TEST(Small_SessionRenderer, TheCutBlockKeepsTheSamplesOfItsRenderedPrefixCleared)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    Buffers buffers(2 * K_BLOCK);
    float* outputs[2] = {buffers.left.data(), buffers.right.data()};

    renderer.render(session, nullptr, outputs, 2 * K_BLOCK);

    const std::vector<float> prefix(buffers.left.begin(), buffers.left.begin() + K_BLOCK);
    EXPECT_EQ(prefix, std::vector<float>(K_BLOCK, 0.0f));
}

class Medium_SessionRendererProfiled : public SushiDSP::profile::test::RecordingBuildTest
{
};

TEST_F(Medium_SessionRendererProfiled, RenderingEightRollingTracksWritesOneLanePerTrack)
{
    const SushiDSP::profile::ProfiledRun run = profile_eight_rolling_tracks();

    ASSERT_TRUE(run.start.started());
    std::vector<std::string> lanes = run.record.lanes();
    std::sort(lanes.begin(), lanes.end());
    const std::vector<std::string> expected = track_lane_names();
    EXPECT_TRUE(std::includes(lanes.begin(), lanes.end(), expected.begin(), expected.end()));
}

TEST_F(Medium_SessionRendererProfiled, RenderingEightRollingTracksCountsTheProcessAndMixZones)
{
    const SushiDSP::profile::ProfiledRun run = profile_eight_rolling_tracks();

    ASSERT_TRUE(run.start.started());
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_PROCESS_ZONE), K_ACTIVE);
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_SESSION_MIX_ZONE), K_ACTIVE * Session::K_MAX_TRACKS);
    EXPECT_EQ(run.record.zone_calls(SushiDSP::profile::names::K_SESSION_METRONOME_ZONE), K_ACTIVE);
}

TEST_F(Medium_SessionRendererProfiled, AProfiledRenderGivesTheSamplesAnUnprofiledOneGives)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    std::vector<float> live(K_BLOCK, 0.25f);
    const float* inputs[2] = {live.data(), live.data()};
    Buffers plain(K_BLOCK);
    float* plain_outputs[2] = {plain.left.data(), plain.right.data()};
    renderer.render(session, inputs, plain_outputs, K_BLOCK);

    Buffers profiled(K_BLOCK);
    float* profiled_outputs[2] = {profiled.left.data(), profiled.right.data()};
    const SushiDSP::profile::ProfiledRun run = SushiDSP::profile::run_profiled(
        SushiDSP::profile::names::K_BENCH_TIMELINE, 0U, 1U, 0U,
        [&]
        {
            renderer.render(session, inputs, profiled_outputs, K_BLOCK);
        });

    ASSERT_TRUE(run.start.started());
    EXPECT_EQ(plain.left, profiled.left);
    EXPECT_EQ(plain.right, profiled.right);
}

namespace
{
    /// @brief Holds the rounded operations a sample of the mix may differ from the oracle by: the
    /// lane gain, the add of the live input, the strip gain and the add into the master.
    constexpr std::size_t K_MIX_ROUNDINGS = 4;

    /// @brief Holds the frame of the block the mix cases read a sample from.
    constexpr std::size_t K_PROBE = 40;

    /// @brief Returns a mono take holding one value in every frame.
    std::unique_ptr<SushiDSP::Take> constant_take(std::size_t frames, float value)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        float* samples = take->mutable_channel(0);
        std::fill(samples, samples + frames, value);
        return take;
    }

    /// @brief Fails unless a sample of the mix is the oracle value to within the rounded operations
    /// of the path; an oracle of zero demands exact silence.
    void expect_sample(float actual, double expected)
    {
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_relative_error<float>(actual, expected, K_MIX_ROUNDINGS)));
    }

    /// @brief Builds a session on the catalogue of the fixture and renders blocks of it.
    class Small_SessionRendererMix : public TestSupport::RigProducts
    {
        protected:
            Small_SessionRendererMix() : session_(K_INFO, catalogue())
            {
                renderer_.prepare(K_BLOCK);
            }

            /// @brief Renders one block into outputs that start filled with a sentinel.
            /// @param live_channel The live input for both sides, or null for none.
            /// @return The two output channels.
            std::array<std::vector<float>, 2> render_block(const float* live_channel)
            {
                std::array<std::vector<float>, 2> out{std::vector<float>(K_BLOCK, 9.0f),
                                                      std::vector<float>(K_BLOCK, 9.0f)};
                float* outputs[2] = {out[0].data(), out[1].data()};
                const float* live[2] = {live_channel, live_channel};
                renderer_.render(session_, live_channel != nullptr ? live : nullptr, outputs,
                                 K_BLOCK);
                return out;
            }

            /// @brief Renders blocks with no live input and returns the last.
            /// @param count How many blocks, at least one.
            std::array<std::vector<float>, 2> render_blocks(int count)
            {
                std::array<std::vector<float>, 2> last = render_block(nullptr);
                for (int block = 1; block < count; ++block)
                {
                    last = render_block(nullptr);
                }
                return last;
            }

            /// @brief Gives lane 0 a take of 0.5 and lane 1 a take of 0.125, turns the second
            /// track hard left, selects it and starts the transport.
            void roll_two_lanes()
            {
                std::string error;
                session_.add_track(error);
                session_.timeline().player().set_take(0, constant_take(4096, 0.5f));
                session_.timeline().player().set_take(1, constant_take(4096, 0.125f));
                TrackStrip second;
                second.pan = -1.0f;
                session_.set_strip(1, second);
                session_.select(1);
                session_.timeline().transport().play();
            }

            /// @brief Puts a mixer at half gain in the first track's rig, which halves what passes
            /// through it, and gives that lane a take of 0.5 under a rolling transport.
            void roll_a_halving_rig()
            {
                const RigController::SlotId mixer = session_.rig(0).append("sushidsp.mixer");
                session_.rig(0).set_parameter(mixer, "gain_0", 0.5f);
                session_.timeline().player().set_take(0, constant_take(8192, 0.5f));
                session_.timeline().transport().play();
            }

            Session session_;
            SessionRenderer renderer_;
    };
} // namespace

TEST_F(Small_SessionRendererMix, TheLiveInputReachesTheSelectedTrackWhileTheTransportIsStopped)
{
    session_.timeline().player().set_take(0, constant_take(4096, 0.5f));
    const std::vector<float> live(K_BLOCK, 0.25f);

    const std::array<std::vector<float>, 2> out = render_block(live.data());

    expect_sample(out[0][K_PROBE], 0.25);
    expect_sample(out[1][K_PROBE], 0.25);
}

TEST_F(Small_SessionRendererMix, AStoppedTransportWithNoLiveInputRendersSilence)
{
    session_.timeline().player().set_take(0, constant_take(4096, 0.5f));

    const std::array<std::vector<float>, 2> out = render_block(nullptr);

    expect_sample(out[0][K_PROBE], 0.0);
    expect_sample(out[1][K_PROBE], 0.0);
}

TEST_F(Small_SessionRendererMix, ARenderedBlockIsCountedInTheAudioTable)
{
    render_block(nullptr);

    EXPECT_EQ(session_.audio().blocks.load(), 1u);
}

// Left carries lane 0 (0.5) and lane 1 with the live input (0.125 + 0.25), both at unity.
TEST_F(Small_SessionRendererMix, RollingSumsEveryLanesRigOutputAndTheLiveInputOnTheLeft)
{
    roll_two_lanes();
    const std::vector<float> live(K_BLOCK, 0.25f);

    const std::array<std::vector<float>, 2> out = render_block(live.data());

    expect_sample(out[0][K_PROBE], 0.5 + 0.125 + 0.25);
}

// A balance of -1 takes the right gain of the second track to 1 + (-1) = 0, leaving lane 0 alone.
TEST_F(Small_SessionRendererMix, AHardLeftPanKeepsATrackOutOfTheRightChannel)
{
    roll_two_lanes();
    const std::vector<float> live(K_BLOCK, 0.25f);

    const std::array<std::vector<float>, 2> out = render_block(live.data());

    expect_sample(out[1][K_PROBE], 0.5);
}

TEST_F(Small_SessionRendererMix, RollingMovesTheTransportByTheBlock)
{
    roll_two_lanes();

    render_block(nullptr);

    EXPECT_EQ(session_.timeline().transport().position_frames(), K_BLOCK);
}

TEST_F(Small_SessionRendererMix, ASoloSilencesTheTracksThatAreNotSoloed)
{
    roll_two_lanes();
    TrackStrip first;
    first.soloed = true;
    session_.set_strip(0, first);

    const std::array<std::vector<float>, 2> out = render_block(nullptr);

    expect_sample(out[0][K_PROBE], 0.5);
}

TEST_F(Small_SessionRendererMix, ABypassedRigPassesItsLaneUnchanged)
{
    roll_a_halving_rig();
    TrackStrip strip;
    strip.rig_bypassed = true;
    session_.set_strip(0, strip);

    const std::array<std::vector<float>, 2> out = render_block(nullptr);

    expect_sample(out[0][K_PROBE], 0.5);
}

// The mixer sums its one input at gain_0 = 0.5, so a lane of 0.5 leaves the rig at 0.25.
TEST_F(Small_SessionRendererMix, AnEngagedRigProcessesItsLane)
{
    roll_a_halving_rig();

    const std::array<std::vector<float>, 2> out = render_blocks(8);

    expect_sample(out[0][K_PROBE], 0.25);
}
