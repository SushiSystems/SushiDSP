/**************************************************************************/
/* test_session_timeline.cpp                                              */
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
 * @file test_session_timeline.cpp
 * @brief Tests the lanes a SessionTimeline adds and removes and the lane names and colours a preset
 * keeps.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rig_persistence.hpp"
#include "session_timeline.hpp"
#include "slot_graph.hpp"
#include "support/rig_products.hpp"

namespace
{
    /// @brief Holds the catalogue the preset codec resolves a screamer against.
    class Small_SessionTimelinePreset : public TestSupport::RigProducts
    {
    };

    /// @brief Names every lane of a timeline "lane <index>".
    void name_every_lane(SessionTimeline& session)
    {
        for (std::size_t index = 0; index < session.track_count(); ++index)
        {
            session.set_track_name(index, "lane " + std::to_string(index));
        }
    }

    /// @brief Fails unless every lane of a timeline holds the palette colour of its index.
    void expect_palette_colors(const SessionTimeline& session)
    {
        for (std::size_t index = 0; index < session.track_count(); ++index)
        {
            EXPECT_EQ(session.track_color(index), SessionTimeline::palette_color(index));
        }
    }
} // namespace

TEST(Small_SessionTimeline, AddAfterRemoveReturnsTheCountToWhereItStarted)
{
    SessionTimeline session({48000.0, 256, 1});
    const std::size_t before = session.track_count();
    ASSERT_GT(before, 0u);
    std::string error;
    ASSERT_TRUE(session.remove_track(before - 1, error)) << error;
    ASSERT_EQ(session.track_count(), before - 1);

    ASSERT_TRUE(session.add_track(error)) << error;

    EXPECT_EQ(session.track_count(), before);
}

TEST(Small_SessionTimeline, AnAddedLaneTakesTheDefaultNameAndThePaletteColor)
{
    SessionTimeline session({48000.0, 256, 1});
    const std::size_t before = session.track_count();
    std::string error;
    ASSERT_TRUE(session.remove_track(before - 1, error)) << error;

    ASSERT_TRUE(session.add_track(error)) << error;

    EXPECT_EQ(session.track_name(before - 1), SessionTimeline::default_track_name(before - 1));
    EXPECT_EQ(session.track_color(before - 1), SessionTimeline::palette_color(before - 1));
}

TEST(Small_SessionTimeline, AddingPastTheLimitIsRefusedWithAReason)
{
    SessionTimeline session({48000.0, 256, 1});
    const std::size_t before = session.track_count();

    std::string error;
    EXPECT_FALSE(session.add_track(error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(session.track_count(), before);
}

TEST(Small_SessionTimeline, RemovingTheMiddleTrackMovesTheOthersDownWithTheirNamesAndColors)
{
    SessionTimeline session({48000.0, 256, 1});
    name_every_lane(session);
    const std::size_t before = session.track_count();
    const SessionTimeline::TrackColor color_of_two = session.track_color(2);
    const SessionTimeline::TrackColor color_of_four = session.track_color(4);

    std::string error;
    ASSERT_TRUE(session.remove_track(3, error)) << error;

    EXPECT_EQ(session.track_count(), before - 1);
    EXPECT_EQ(session.track_name(2), "lane 2");
    EXPECT_EQ(session.track_color(2), color_of_two);
    EXPECT_EQ(session.track_name(3), "lane 4");
    EXPECT_EQ(session.track_color(3), color_of_four);
}

TEST_F(Small_SessionTimelinePreset, TheTrackCountSurvivesAPresetRoundTrip)
{
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SlotGraph graph(info, {}, catalogue());
    SessionTimeline session(info);
    RigPersistence codec(info, graph, session, catalogue());
    graph.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(session.remove_track(session.track_count() - 1, error)) << error;
    const std::string json_text = codec.save_to_json();

    SlotGraph reloaded_graph(info, {}, catalogue());
    SessionTimeline reloaded(info);
    RigPersistence reloaded_codec(info, reloaded_graph, reloaded, catalogue());
    reloaded_codec.load_from_json(json_text);

    EXPECT_EQ(reloaded.track_count(), session.track_count());
}

TEST_F(Small_SessionTimelinePreset, TheTrackNameAndColorSurviveAPresetRoundTrip)
{
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SlotGraph graph(info, {}, catalogue());
    SessionTimeline session(info);
    RigPersistence codec(info, graph, session, catalogue());
    graph.append("sushidsp.tubescreamer");
    session.set_track_name(1, "rhythm");
    session.set_track_color(1, 0x123456FFu);
    const std::string json_text = codec.save_to_json();

    SlotGraph reloaded_graph(info, {}, catalogue());
    SessionTimeline reloaded(info);
    RigPersistence reloaded_codec(info, reloaded_graph, reloaded, catalogue());
    reloaded_codec.load_from_json(json_text);

    EXPECT_EQ(reloaded.track_name(1), "rhythm");
    EXPECT_EQ(reloaded.track_color(1), 0x123456FFu);
}

TEST_F(Small_SessionTimelinePreset, APresetWithoutColorsLoadsThePaletteColors)
{
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SlotGraph graph(info, {}, catalogue());
    SessionTimeline session(info);
    RigPersistence codec(info, graph, session, catalogue());
    graph.append("sushidsp.tubescreamer");
    session.set_track_name(0, "rhythm");
    nlohmann::json document = nlohmann::json::parse(codec.save_to_json());
    document.erase("track_count");
    for (nlohmann::json& track_json : document.at("tracks"))
    {
        track_json.erase("color");
    }

    SlotGraph reloaded_graph(info, {}, catalogue());
    SessionTimeline reloaded(info);
    RigPersistence reloaded_codec(info, reloaded_graph, reloaded, catalogue());
    reloaded_codec.load_from_json(document.dump());

    EXPECT_EQ(reloaded.track_count(), SessionTimeline::K_MAX_TRACKS);
    EXPECT_EQ(reloaded.track_name(0), "rhythm");
    expect_palette_colors(reloaded);
}

TEST_F(Small_SessionTimelinePreset, APresetWithAMalformedColorLeavesTheTimelineAlone)
{
    const SushiDSP::PrepareInfo info{48000.0, 256, 1};
    SlotGraph graph(info, {}, catalogue());
    SessionTimeline session(info);
    RigPersistence codec(info, graph, session, catalogue());
    graph.append("sushidsp.tubescreamer");
    session.set_track_name(0, "rhythm");
    session.set_track_color(0, 0x123456FFu);
    nlohmann::json document = nlohmann::json::parse(codec.save_to_json());
    document.at("tracks").at(0).at("color") = nlohmann::json::array({1, 2});

    EXPECT_THROW(codec.load_from_json(document.dump()), std::runtime_error);

    EXPECT_EQ(session.track_name(0), "rhythm");
    EXPECT_EQ(session.track_color(0), 0x123456FFu);
    EXPECT_EQ(session.track_count(), SessionTimeline::K_MAX_TRACKS);
}
