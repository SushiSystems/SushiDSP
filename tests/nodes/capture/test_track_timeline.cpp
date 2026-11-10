/**************************************************************************/
/* test_track_timeline.cpp                                                */
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
 * @file test_track_timeline.cpp
 * @brief Tests how TrackTimeline orders, refuses and fades its clips.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/take.hpp>
#include <SushiDSP/nodes/capture/take_library.hpp>
#include <SushiDSP/nodes/capture/track_timeline.hpp>

#include "support/tolerance.hpp"

namespace
{
    /// @brief Names a frame and the index of the first clip that ends after it.
    struct FindRow
    {
        const char* name;
        std::uint64_t frame;
        std::size_t index;
    };

    /// @brief Names a clip length, a frame in it and the edge gain expected there.
    struct EdgeRow
    {
        const char* name;
        std::uint64_t length;
        std::uint64_t frame;
        float gain;
    };

    /// @brief Builds a clip naming a take, at a timeline frame, for a length.
    SushiDSP::Clip make_clip(const SushiDSP::Take* take, std::uint64_t start_frame,
                             std::uint64_t length_frames)
    {
        return SushiDSP::Clip{take, start_frame, 0u, length_frames};
    }

    /// @brief Inserts a one-second mono take into the library.
    const SushiDSP::Take* insert_take(SushiDSP::TakeLibrary& library)
    {
        return library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));
    }

    /// @brief Builds count clips of 500 frames, one every 1000 frames.
    std::vector<SushiDSP::Clip> spaced_clips(const SushiDSP::Take* take, std::uint64_t count)
    {
        std::vector<SushiDSP::Clip> clips;
        for (std::uint64_t index = 0; index < count; ++index)
        {
            clips.push_back(make_clip(take, index * 1000u, 500u));
        }
        return clips;
    }

    /// @brief Builds two clips that overlap by 2000 frames.
    std::vector<SushiDSP::Clip> overlapping_clips(const SushiDSP::Take* take)
    {
        return {make_clip(take, 1000u, 5000u), make_clip(take, 4000u, 5000u)};
    }

    /// @brief Returns the text of the refusal a timeline over the clips gives, or an empty text
    /// when it is not refused.
    std::string refusal_message(SushiDSP::TakeLibrary& library,
                                const std::vector<SushiDSP::Clip>& clips)
    {
        try
        {
            const SushiDSP::TrackTimeline timeline(library, clips);
        }
        catch (const std::runtime_error& refusal)
        {
            return refusal.what();
        }
        return std::string();
    }

    /// @brief Expects an edge gain within the rounding of the operations that make it.
    void expect_edge_gain(const SushiDSP::Clip& clip, std::uint64_t frame, float expected)
    {
        EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
            SushiDSP::TrackTimeline::edge_gain(clip, frame), expected, 2)))
            << "frame " << frame;
    }

    /// @brief Expects the gain at every frame of a clip to lie in [0, 1] and to mirror the gain at
    /// the frame the same distance from the other end.
    void expect_ramps_within_range_and_symmetric(const SushiDSP::Clip& clip)
    {
        for (std::uint64_t frame = 0; frame <= clip.length_frames; ++frame)
        {
            const float gain = SushiDSP::TrackTimeline::edge_gain(clip, frame);
            EXPECT_LE(gain, 1.0f) << "frame " << frame;
            EXPECT_GE(gain, 0.0f) << "frame " << frame;
            expect_edge_gain(clip, clip.length_frames - frame, gain);
        }
    }
} // namespace

TEST(Small_TrackTimeline, TrackTimelineSortsItsClipsByStartFrame)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips{
        make_clip(take, 96000u, 1000u), make_clip(take, 0u, 1000u), make_clip(take, 48000u, 1000u)};

    const SushiDSP::TrackTimeline timeline(library, clips);

    ASSERT_EQ(timeline.clip_count(), 3u);
    EXPECT_EQ(timeline.clip(0).start_frame, 0u);
    EXPECT_EQ(timeline.clip(1).start_frame, 48000u);
    EXPECT_EQ(timeline.clip(2).start_frame, 96000u);
}

TEST(Small_TrackTimeline, TrackTimelineRefusesTwoOverlappingClipsAndNamesThem)
{
    SushiDSP::TakeLibrary library;
    const std::vector<SushiDSP::Clip> clips = overlapping_clips(insert_take(library));

    const std::string message = refusal_message(library, clips);

    EXPECT_NE(message.find("1000"), std::string::npos) << message;
    EXPECT_NE(message.find("4000"), std::string::npos) << message;
}

TEST(Small_TrackTimeline, TrackTimelineRetainsNoTakeWhenItRefusesOverlappingClips)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips = overlapping_clips(take);

    refusal_message(library, clips);

    EXPECT_EQ(library.reference_count(take), 0u);
}

TEST(Small_TrackTimeline, TrackTimelineAcceptsSixteenClips)
{
    SushiDSP::TakeLibrary library;
    const std::vector<SushiDSP::Clip> clips = spaced_clips(insert_take(library), 16u);

    const SushiDSP::TrackTimeline timeline(library, clips);

    EXPECT_EQ(timeline.clip_count(), 16u);
}

TEST(Small_TrackTimeline, TrackTimelineRefusesMoreThanSixteenClips)
{
    SushiDSP::TakeLibrary library;
    const std::vector<SushiDSP::Clip> clips = spaced_clips(insert_take(library), 17u);

    EXPECT_THROW((SushiDSP::TrackTimeline{library, clips}), std::runtime_error);
}

TEST(Small_TrackTimeline, TrackTimelineRetainsNoTakeWhenItRefusesMoreThanSixteenClips)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips = spaced_clips(take, 17u);

    refusal_message(library, clips);

    EXPECT_EQ(library.reference_count(take), 0u);
}

class Small_TrackTimelineFind : public ::testing::TestWithParam<FindRow>
{
};

TEST_P(Small_TrackTimelineFind, TrackTimelineFindsTheFirstClipPastAFrame)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips{
        make_clip(take, 0u, 1000u), make_clip(take, 5000u, 1000u), make_clip(take, 9000u, 1000u)};
    const SushiDSP::TrackTimeline timeline(library, clips);

    EXPECT_EQ(timeline.first_clip_at_or_after(GetParam().frame), GetParam().index);
}

INSTANTIATE_TEST_SUITE_P(
    Rows, Small_TrackTimelineFind,
    ::testing::Values(FindRow{"AtTheFirstFrame", 0u, 0u}, FindRow{"InsideTheFirstClip", 999u, 0u},
                      FindRow{"AtTheEndOfTheFirstClip", 1000u, 1u},
                      FindRow{"InsideTheSecondClip", 5500u, 1u},
                      FindRow{"AtTheEndOfTheSecondClip", 6000u, 2u},
                      FindRow{"PastEveryClip", 10000u, 3u}),
    [](const ::testing::TestParamInfo<FindRow>& info) { return std::string(info.param.name); });

TEST(Small_TrackTimeline, TrackTimelineRetainsEachDistinctTakeOnce)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips{make_clip(take, 0u, 1000u), make_clip(take, 5000u, 1000u)};

    const SushiDSP::TrackTimeline timeline(library, clips);

    EXPECT_EQ(library.reference_count(take), 1u);
}

TEST(Small_TrackTimeline, TrackTimelineReleasesItsTakesWhenItIsDestroyed)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take = insert_take(library);
    const std::vector<SushiDSP::Clip> clips{make_clip(take, 0u, 1000u)};
    auto timeline = std::make_unique<const SushiDSP::TrackTimeline>(library, clips);

    timeline.reset();
    library.collect();

    EXPECT_EQ(library.reference_count(take), 0u);
    EXPECT_EQ(library.size(), 0u);
}

class Small_TrackTimelineEdge : public ::testing::TestWithParam<EdgeRow>
{
};

TEST_P(Small_TrackTimelineEdge, TrackTimelineFadesFiveMillisecondsAtEachEdge)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Clip clip = make_clip(insert_take(library), 0u, GetParam().length);

    expect_edge_gain(clip, GetParam().frame, GetParam().gain);
}

/// 5 ms at 48 kHz is 240 frames; a 100 frame clip is shorter than 2 * K_FADE_FRAMES, so each ramp is 50.
INSTANTIATE_TEST_SUITE_P(
    Rows, Small_TrackTimelineEdge,
    ::testing::Values(EdgeRow{"LongClipStart", 48000u, 0u, 0.0f},
                      EdgeRow{"LongClipHalfWayUp", 48000u, 120u, 0.5f},
                      EdgeRow{"LongClipTopOfTheRamp", 48000u, 240u, 1.0f},
                      EdgeRow{"LongClipMiddle", 48000u, 24000u, 1.0f},
                      EdgeRow{"LongClipStartOfTheRampDown", 48000u, 47760u, 1.0f},
                      EdgeRow{"LongClipHalfWayDown", 48000u, 47880u, 0.5f},
                      EdgeRow{"LongClipEnd", 48000u, 48000u, 0.0f},
                      EdgeRow{"ShortClipStart", 100u, 0u, 0.0f},
                      EdgeRow{"ShortClipQuarter", 100u, 25u, 0.5f},
                      EdgeRow{"ShortClipMiddle", 100u, 50u, 1.0f},
                      EdgeRow{"ShortClipThreeQuarters", 100u, 75u, 0.5f},
                      EdgeRow{"ShortClipEnd", 100u, 100u, 0.0f}),
    [](const ::testing::TestParamInfo<EdgeRow>& info) { return std::string(info.param.name); });

TEST(Small_TrackTimeline, TrackTimelineFadesAShortClipWithoutOverlappingItsOwnRamps)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Clip clip = make_clip(insert_take(library), 0u, 100u);

    expect_ramps_within_range_and_symmetric(clip);
}
