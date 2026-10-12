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
 * @file test_track_timeline.cpp
 * @brief Tests the clip ordering and limits of TrackTimeline.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/capture/take_library.hpp"
#include "SushiDSP/capture/track_timeline.hpp"

namespace
{
    /// @brief Builds a clip naming a take, at a timeline frame, for a length.
    SushiDSP::Clip make_clip(const SushiDSP::Take* take, std::uint64_t start_frame,
                             std::uint64_t length_frames)
    {
        return SushiDSP::Clip{take, start_frame, 0u, length_frames};
    }
} // namespace

TEST(Unit_Dsp, TrackTimelineSortsItsClipsByStartFrame)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    const std::vector<SushiDSP::Clip> clips{
        make_clip(take, 96000u, 1000u), make_clip(take, 0u, 1000u), make_clip(take, 48000u, 1000u)};

    const SushiDSP::TrackTimeline timeline(library, clips);

    ASSERT_EQ(timeline.clip_count(), 3u);
    EXPECT_EQ(timeline.clip(0).start_frame, 0u);
    EXPECT_EQ(timeline.clip(1).start_frame, 48000u);
    EXPECT_EQ(timeline.clip(2).start_frame, 96000u);
}

TEST(Unit_Dsp, TrackTimelineRefusesTwoOverlappingClipsAndNamesThem)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    const std::vector<SushiDSP::Clip> clips{make_clip(take, 1000u, 5000u),
                                            make_clip(take, 4000u, 5000u)};

    try
    {
        const SushiDSP::TrackTimeline timeline(library, clips);
        FAIL() << "two overlapping clips must be refused";
    }
    catch (const std::runtime_error& refusal)
    {
        const std::string message(refusal.what());
        EXPECT_NE(message.find("1000"), std::string::npos) << message;
        EXPECT_NE(message.find("4000"), std::string::npos) << message;
    }

    EXPECT_EQ(library.reference_count(take), 0u);
}

TEST(Unit_Dsp, TrackTimelineRefusesMoreThanSixteenClips)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    std::vector<SushiDSP::Clip> clips;

    for (std::uint64_t index = 0; index < 17u; ++index)
    {
        clips.push_back(make_clip(take, index * 1000u, 500u));
    }

    EXPECT_THROW((SushiDSP::TrackTimeline{library, clips}), std::runtime_error);
    EXPECT_EQ(library.reference_count(take), 0u);
}

TEST(Unit_Dsp, TrackTimelineFindsTheFirstClipPastAFrame)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    const std::vector<SushiDSP::Clip> clips{
        make_clip(take, 0u, 1000u), make_clip(take, 5000u, 1000u), make_clip(take, 9000u, 1000u)};

    const SushiDSP::TrackTimeline timeline(library, clips);

    EXPECT_EQ(timeline.first_clip_at_or_after(0u), 0u);
    EXPECT_EQ(timeline.first_clip_at_or_after(999u), 0u);
    EXPECT_EQ(timeline.first_clip_at_or_after(1000u), 1u); // the first clip ends at 1000
    EXPECT_EQ(timeline.first_clip_at_or_after(5500u), 1u);
    EXPECT_EQ(timeline.first_clip_at_or_after(6000u), 2u);
    EXPECT_EQ(timeline.first_clip_at_or_after(10000u), 3u); // clip_count(), nothing reaches past
}

TEST(Unit_Dsp, TrackTimelineRetainsEachDistinctTakeOnce)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    {
        const std::vector<SushiDSP::Clip> clips{make_clip(take, 0u, 1000u),
                                                make_clip(take, 5000u, 1000u)};
        const SushiDSP::TrackTimeline timeline(library, clips);

        EXPECT_EQ(library.reference_count(take), 1u);
    }

    EXPECT_EQ(library.reference_count(take), 0u);
    library.collect();
    EXPECT_EQ(library.size(), 0u);
}

TEST(Unit_Dsp, TrackTimelineFadesFiveMillisecondsAtEachEdge)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    const SushiDSP::Clip clip = make_clip(take, 0u, 48000u);

    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 0u), 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 120u), 0.5f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 240u), 1.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 24000u), 1.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 47760u), 1.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 47880u), 0.5f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 48000u), 0.0f);
}

TEST(Unit_Dsp, TrackTimelineFadesAShortClipWithoutOverlappingItsOwnRamps)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 48000u));

    // 100 frames is shorter than 2 * K_FADE_FRAMES, so each ramp is 50 frames long.
    const SushiDSP::Clip clip = make_clip(take, 0u, 100u);

    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 0u), 0.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 25u), 0.5f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 50u), 1.0f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 75u), 0.5f);
    EXPECT_FLOAT_EQ(SushiDSP::TrackTimeline::edge_gain(clip, 100u), 0.0f);

    for (std::uint64_t frame = 0; frame <= 100u; ++frame)
    {
        const float gain = SushiDSP::TrackTimeline::edge_gain(clip, frame);
        EXPECT_LE(gain, 1.0f) << "frame " << frame;
        EXPECT_GE(gain, 0.0f) << "frame " << frame;
        EXPECT_FLOAT_EQ(gain, SushiDSP::TrackTimeline::edge_gain(clip, 100u - frame))
            << "frame " << frame;
    }
}
