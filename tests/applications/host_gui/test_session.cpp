/**************************************************************************/
/* test_session.cpp                                                       */
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

/**
 * @file test_session.cpp
 * @brief Tests the gains a track strip gives and the tracks a Session adds, removes and selects.
 * @author Mustafa Garip
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "session.hpp"
#include "support/rig_products.hpp"
#include "support/tolerance.hpp"

namespace
{
    /// @brief Holds what every session of the tests is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, 64, 2};

    /// @brief Holds a strip, whether any track is soloed, the two gains the strip must give and
    /// the rounded operations the gains may differ from the oracle by.
    struct StripGainRow
    {
            const char* name;
            TrackStrip strip;
            bool any_soloed;
            double left;
            double right;
            std::size_t roundings;
    };

    /// @brief Returns a strip with one field set, the rest at their defaults.
    TrackStrip strip_with(float level_db, float pan, bool muted, bool soloed)
    {
        TrackStrip strip;
        strip.level_db = level_db;
        strip.pan = pan;
        strip.muted = muted;
        strip.soloed = soloed;
        return strip;
    }

    /// @brief Returns the linear gain of a fader, 10^(decibels / 20), in double precision.
    double fader_gain(float level_db)
    {
        return std::pow(10.0, static_cast<double>(level_db) / 20.0);
    }

    /// @brief Fails unless a strip gain is the oracle gain to within the row's rounded operations.
    void expect_gain(float actual, double expected, std::size_t roundings)
    {
        EXPECT_TRUE(TestSupport::is_within(
            TestSupport::measure_relative_error<float>(actual, expected, roundings)));
    }

    /// @brief Holds the rows of strip gains.
    class Small_StripGains : public ::testing::TestWithParam<StripGainRow>
    {
    };

    /// @brief Builds a session on the catalogue of the fixture.
    class Small_SessionTracks : public TestSupport::RigProducts
    {
        protected:
            Small_SessionTracks() : session_(K_INFO, catalogue())
            {
            }

            /// @brief Adds tracks until the session holds the most it can.
            void add_tracks_up_to_the_limit()
            {
                std::string ignored;
                while (session_.track_count() < Session::K_MAX_TRACKS)
                {
                    session_.add_track(ignored);
                }
            }

            Session session_;
    };
} // namespace

// The rounded operations of each row are derived in the README of this folder.
TEST_P(Small_StripGains, AStripGivesTheLeftAndRightGainOfItsRow)
{
    const StripGainRow& row = GetParam();

    const std::array<float, 2> gains = strip_gains(row.strip, row.any_soloed);

    expect_gain(gains[0], row.left, row.roundings);
    expect_gain(gains[1], row.right, row.roundings);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_StripGains,
    ::testing::Values(
        StripGainRow{"UnityAtTheCentre", TrackStrip{}, false, 1.0, 1.0, 0},
        StripGainRow{"FaderBelowUnity", strip_with(-6.0206f, 0.0f, false, false), false,
                     fader_gain(-6.0206f), fader_gain(-6.0206f), 3},
        StripGainRow{"BalanceTakesTheFarSideDown", strip_with(0.0f, 0.5f, false, false), false,
                     0.5, 1.0, 0},
        StripGainRow{"HardLeftSilencesTheRight", strip_with(0.0f, -1.0f, false, false), false,
                     1.0, 0.0, 0},
        StripGainRow{"BalancePastTheEndIsClamped", strip_with(0.0f, -3.0f, false, false), false,
                     1.0, 0.0, 0},
        StripGainRow{"MuteSilencesTheTrack", strip_with(0.0f, 0.0f, true, false), false, 0.0, 0.0,
                     0},
        StripGainRow{"AnotherSoloSilencesTheTrack", strip_with(0.0f, 0.0f, false, false), true,
                     0.0, 0.0, 0},
        StripGainRow{"ASoloedTrackIgnoresItsMute", strip_with(0.0f, 0.0f, true, true), true, 1.0,
                     1.0, 0}),
    [](const ::testing::TestParamInfo<StripGainRow>& info)
    {
        return std::string(info.param.name);
    });

TEST_F(Small_SessionTracks, ASessionOpensWithOneTrackOnTheFirstLane)
{
    EXPECT_EQ(session_.track_count(), 1u);
    EXPECT_EQ(session_.timeline().track_count(), 1u);
    EXPECT_EQ(session_.selected(), 0u);
}

TEST_F(Small_SessionTracks, EachAddedTrackGetsARigOfItsOwnInTheAudioTable)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    ASSERT_TRUE(session_.add_track(error)) << error;

    EXPECT_EQ(session_.track_count(), 3u);
    EXPECT_NE(&session_.rig(0), &session_.rig(1));
    EXPECT_EQ(session_.audio().tracks[2].rig.load(), &session_.rig(2));
    EXPECT_EQ(session_.audio().tracks[3].rig.load(), nullptr);
}

TEST_F(Small_SessionTracks, EveryRigReadsTheOneTimeline)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    ASSERT_TRUE(session_.add_track(error)) << error;

    EXPECT_EQ(session_.rig(1).track_count(), 3u);
}

TEST_F(Small_SessionTracks, ASlotAppendedToOneTracksRigLeavesTheOtherRigsEmpty)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;

    session_.rig(1).append("sushidsp.tubescreamer");

    EXPECT_EQ(session_.rig(1).slot_order().size(), 1u);
    EXPECT_TRUE(session_.rig(0).slot_order().empty());
}

TEST_F(Small_SessionTracks, RemovingATrackMovesTheTracksAfterItDown)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    RigController* const third = &session_.rig(2);

    ASSERT_TRUE(session_.remove_track(1, false, error)) << error;

    EXPECT_EQ(session_.track_count(), 2u);
    EXPECT_EQ(&session_.rig(1), third);
    EXPECT_EQ(session_.audio().tracks[1].rig.load(), third);
    EXPECT_EQ(session_.audio().tracks[2].rig.load(), nullptr);
}

TEST_F(Small_SessionTracks, RemovingATrackBelowTheSelectedOneKeepsTheSelectionOnItsTrack)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    session_.select(2);

    ASSERT_TRUE(session_.remove_track(1, false, error)) << error;

    EXPECT_EQ(session_.selected(), 1u);
}

TEST_F(Small_SessionTracks, RemovingATrackMovesTheRigRevision)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    const std::uint64_t before = session_.rig_revision();

    ASSERT_TRUE(session_.remove_track(1, false, error)) << error;

    EXPECT_NE(session_.rig_revision(), before);
}

TEST_F(Small_SessionTracks, TheOnlyTrackIsNeverRemoved)
{
    std::string error;

    EXPECT_FALSE(session_.remove_track(0, false, error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session_.track_count(), 1u);
}

TEST_F(Small_SessionTracks, ATrackPastTheLastIsNotRemoved)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;

    EXPECT_FALSE(session_.remove_track(5, false, error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session_.track_count(), 2u);
}

TEST_F(Small_SessionTracks, ANinthTrackIsRefusedWithAReason)
{
    add_tracks_up_to_the_limit();
    ASSERT_EQ(session_.track_count(), Session::K_MAX_TRACKS);
    std::string error;

    EXPECT_FALSE(session_.add_track(error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session_.track_count(), Session::K_MAX_TRACKS);
}
