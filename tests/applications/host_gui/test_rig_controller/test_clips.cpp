/**************************************************************************/
/* test_clips.cpp                                                         */
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
 * @file test_clips.cpp
 * @brief Tests how RigController places, moves and records clips on a lane and how a preset keeps
 * them with the tempo.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rig_controller.hpp"
#include "support/rig_products.hpp"
#include "support/wav_fixture.hpp"
#include <SushiDSP/nodes/capture/transport.hpp>

namespace
{
    /// @brief Holds the rigs the clip tests build, on the catalogue of the fixture.
    class Medium_RigControllerClips : public TestSupport::RigProducts
    {
    };

    /// @brief Places clips of one file on a lane, one every 512 frames from frame 0.
    /// @param controller The rig.
    /// @param lane The lane.
    /// @param path The take's file.
    /// @param count How many clips to place.
    /// @return How many clips the rig took.
    std::size_t place_clips_every_512_frames(RigController& controller, std::size_t lane,
                                             const std::string& path, std::size_t count)
    {
        std::size_t placed = 0;
        std::string error;
        for (std::size_t index = 0; index < count; ++index)
        {
            if (controller.place_clip(lane, path, index * 512u, error))
            {
                ++placed;
            }
        }
        return placed;
    }

    /// @brief Writes one block of dry take into the rig's recording bus: 0.5 and -0.25 first, then
    /// silence.
    /// @return True when the bus took the block.
    bool write_dry_block(RigController& controller, std::uint32_t block)
    {
        std::vector<float> dry(block, 0.0f);
        dry[0] = 0.5f;
        dry[1] = -0.25f;
        std::vector<float> left(block, 0.0f);
        std::vector<float> right(block, 0.0f);
        const float* processed[2] = {left.data(), right.data()};
        return controller.recording_bus().write(processed, dry.data(), block);
    }

    /// @brief Builds a preset written before the arrangement existed: one screamer and one track
    /// row, lane 2 "rhythm" at gain 0.5, whose take is @p path.
    std::string pre_dt_preset(const std::string& path)
    {
        nlohmann::json preset = nlohmann::json::parse(
            R"({"nodes":[{"id":0,"type":"sushidsp.tubescreamer","params":{},"inputs":[]}]})");
        nlohmann::json row = nlohmann::json::object();
        row["index"] = 2;
        row["name"] = "rhythm";
        row["path"] = path;
        row["gain"] = 0.5f;
        row["pan"] = -0.25f;
        row["muted"] = false;
        row["soloed"] = false;
        preset["tracks"] = nlohmann::json::array({row});
        return preset.dump();
    }
} // namespace

TEST_F(Medium_RigControllerClips, PlacesAClipAtTheFrameAsked)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    ASSERT_TRUE(controller.place_clip(1, path, 12345, error)) << error;

    ASSERT_EQ(controller.clip_count(1), 1u);
    EXPECT_EQ(controller.clip(1, 0).path, path);
    EXPECT_EQ(controller.clip(1, 0).start_frame, 12345u);
    EXPECT_EQ(controller.clip(1, 0).take_offset, 0u);
    EXPECT_EQ(controller.clip(1, 0).length_frames, 256u);
    EXPECT_TRUE(controller.track_is_loaded(1));
    EXPECT_EQ(controller.track_name(1), TestSupport::path_stem_of(path));
}

TEST_F(Medium_RigControllerClips, RefusesAFileThatWillNotParseWithAReason)
{
    const std::string path = TestSupport::write_fixture_bytes(
        "not_a_wav.wav", std::vector<std::uint8_t>{'t', 'h', 'i', 's'});
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    EXPECT_FALSE(controller.place_clip(0, path, 480, error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.clip_count(0), 0u);
    EXPECT_FALSE(controller.track_is_loaded(0));
}

TEST_F(Medium_RigControllerClips, RefusesAClipPastTheLaneLimit)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    const std::size_t limit = SushiDSP::TrackTimeline::K_MAX_CLIPS;
    ASSERT_EQ(place_clips_every_512_frames(controller, 0, path, limit), limit);

    std::string error;
    EXPECT_FALSE(controller.place_clip(0, path, limit * 512u, error));

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(controller.clip_count(0), limit);
}

TEST_F(Medium_RigControllerClips, PlacesTwoClipsFromOneFileWithOneTakeInMemory)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});

    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 0, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 4800, error)) << error;

    EXPECT_EQ(controller.clip_count(0), 2u);
    EXPECT_EQ(controller.player().takes().size(), 1u);
}

TEST_F(Medium_RigControllerClips, RefusesAClipThatOverlapsOneTheLaneHolds)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;

    EXPECT_FALSE(controller.place_clip(0, path, 1100, error));

    EXPECT_NE(error.find("overlap"), std::string::npos) << error;
    EXPECT_EQ(controller.clip_count(0), 1u);
    EXPECT_EQ(controller.clip(0, 0).start_frame, 1000u);
}

TEST_F(Medium_RigControllerClips, MovingAClipKeepsTheLaneSortedByStartFrame)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 0, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;

    ASSERT_TRUE(controller.move_clip(0, 0, 5000, error)) << error;

    EXPECT_EQ(controller.clip(0, 0).start_frame, 1000u);
    EXPECT_EQ(controller.clip(0, 1).start_frame, 5000u);
}

TEST_F(Medium_RigControllerClips, RefusesToMoveAClipOntoAnother)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 5000, error)) << error;

    EXPECT_FALSE(controller.move_clip(0, 1, 900, error));

    EXPECT_NE(error.find("overlap"), std::string::npos) << error;
    EXPECT_EQ(controller.clip(0, 1).start_frame, 5000u);
}

TEST_F(Medium_RigControllerClips, RemovingAClipLeavesTheOthersInOrder)
{
    const std::string path =
        TestSupport::write_temp_float_wav("clip.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    std::string error;
    ASSERT_TRUE(controller.place_clip(0, path, 1000, error)) << error;
    ASSERT_TRUE(controller.place_clip(0, path, 5000, error)) << error;

    ASSERT_TRUE(controller.remove_clip(0, 0, error)) << error;

    ASSERT_EQ(controller.clip_count(0), 1u);
    EXPECT_EQ(controller.clip(0, 0).start_frame, 5000u);
}

TEST_F(Medium_RigControllerClips, StopsARecordingIntoAClipAtTheArmedFrame)
{
    RigController controller = rig({48000.0, 64, 2});
    controller.set_take_folder(::testing::TempDir());
    controller.set_record_track(2);
    controller.transport().seek(12345);
    std::string error;
    ASSERT_TRUE(controller.arm_recording("RigControllerStopsARecording", error)) << error;
    ASSERT_EQ(controller.armed_frame(), 12345u);
    ASSERT_TRUE(write_dry_block(controller, 64));

    controller.stop_recording(&error);

    EXPECT_TRUE(error.empty()) << error;
    ASSERT_EQ(controller.clip_count(2), 1u);
    EXPECT_EQ(controller.clip(2, 0).start_frame, 12345u);
    EXPECT_EQ(controller.clip(2, 0).length_frames, 64u);
    EXPECT_NE(controller.clip(2, 0).path.find("-dry.wav"), std::string::npos);
}

TEST_F(Medium_RigControllerClips, ReAmpsARecordedTakeFromTheBarItWasRecordedAt)
{
    const std::uint32_t block = 64;
    RigController controller = rig({48000.0, block, 2});
    controller.set_take_folder(::testing::TempDir());
    controller.set_record_track(0);
    // Bar 5 at 120 bpm in 4/4 and 48 kHz is 4 bars of 2 s: 8 s, 384000 frames.
    const std::uint64_t bar_five = controller.musical_clock().frame_of(5, 1);
    ASSERT_EQ(bar_five, 384000u);
    controller.transport().seek(bar_five);
    std::string error;
    ASSERT_TRUE(controller.arm_recording("RigControllerReAmpsARecordedTake", error)) << error;
    ASSERT_TRUE(write_dry_block(controller, block));
    controller.stop_recording(&error);
    ASSERT_TRUE(error.empty()) << error;
    ASSERT_EQ(controller.clip_count(0), 1u);
    ASSERT_EQ(controller.clip(0, 0).start_frame, bar_five);
    ASSERT_TRUE(controller.set_reamp_track(0, error)) << error;
    std::vector<float> assembled(block, 0.0f);
    float* assembled_pointer[1] = {assembled.data()};
    controller.transport().play();

    controller.transport().seek(0);
    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());
    TestSupport::expect_same_signal(assembled, std::vector<float>(block, 0.0f));

    std::fill(assembled.begin(), assembled.end(), 0.0f);
    controller.transport().seek(bar_five);
    controller.reamp_player().process(assembled_pointer, 1, block, controller.transport());
    EXPECT_EQ(assembled[0], 0.5f);
    EXPECT_EQ(assembled[1], -0.25f);
}

TEST_F(Medium_RigControllerClips, RoundTripsTheClipsOfTwoLanesThroughJson)
{
    const std::string first =
        TestSupport::write_temp_float_wav("clip_a.wav", TestSupport::make_test_signal(256), 48000);
    const std::string second =
        TestSupport::write_temp_float_wav("clip_b.wav", TestSupport::make_test_signal(128), 48000);
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(source.place_clip(0, first, 0, error)) << error;
    ASSERT_TRUE(source.place_clip(0, second, 96000, error)) << error;
    ASSERT_TRUE(source.place_clip(3, first, 48000, error)) << error;
    ASSERT_TRUE(source.place_clip(3, second, 192000, error)) << error;
    source.set_track_name(3, "lead");
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(source.save_to_json());

    ASSERT_EQ(target.clip_count(0), 2u);
    ASSERT_EQ(target.clip_count(3), 2u);
    EXPECT_EQ(target.clip(0, 0).path, first);
    EXPECT_EQ(target.clip(0, 1).start_frame, 96000u);
    EXPECT_EQ(target.clip(0, 1).length_frames, 128u);
    EXPECT_EQ(target.clip(3, 0).start_frame, 48000u);
    EXPECT_EQ(target.clip(3, 1).start_frame, 192000u);
    EXPECT_EQ(target.track_name(3), "lead");
}

TEST_F(Medium_RigControllerClips, SavingALoadedPresetWritesTheSameText)
{
    const std::string first =
        TestSupport::write_temp_float_wav("clip_a.wav", TestSupport::make_test_signal(256), 48000);
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    std::string error;
    ASSERT_TRUE(source.place_clip(0, first, 0, error)) << error;
    source.transport().set_loop(4800, 9600);
    const std::string saved = source.save_to_json();
    RigController target = rig({48000.0, 64, 2});
    target.load_from_json(saved);

    EXPECT_EQ(target.save_to_json(), saved);
}

TEST_F(Medium_RigControllerClips, RoundTripsTheTempoAndTheMeterThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    source.musical_clock().set_tempo(137.5);
    source.musical_clock().set_meter(7, 8);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(source.save_to_json());

    EXPECT_EQ(target.musical_clock().beats_per_minute(), 137.5);
    EXPECT_EQ(target.musical_clock().beats_per_bar(), 7u);
    EXPECT_EQ(target.musical_clock().beat_unit(), 8u);
}

TEST_F(Medium_RigControllerClips, RoundTripsTheMetronomeThroughJson)
{
    RigController source = rig({48000.0, 64, 2});
    source.append("sushidsp.tubescreamer");
    source.metronome().set_enabled(true);
    source.metronome().set_level(0.25f);
    RigController target = rig({48000.0, 64, 2});

    target.load_from_json(source.save_to_json());

    EXPECT_TRUE(target.metronome().enabled());
    EXPECT_EQ(target.metronome().level(), 0.25f);
}

TEST_F(Medium_RigControllerClips, LoadsAPreDtTrackRowAsOneClipAtFrameZero)
{
    const std::string path =
        TestSupport::write_temp_float_wav("legacy.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(pre_dt_preset(path));

    ASSERT_EQ(controller.clip_count(2), 1u);
    EXPECT_EQ(controller.clip(2, 0).path, path);
    EXPECT_EQ(controller.clip(2, 0).start_frame, 0u);
    EXPECT_EQ(controller.clip(2, 0).take_offset, 0u);
    EXPECT_EQ(controller.clip(2, 0).length_frames, 256u);
}

TEST_F(Medium_RigControllerClips, LoadsAPreDtTrackRowWithItsNameAndGain)
{
    const std::string path =
        TestSupport::write_temp_float_wav("legacy.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});

    controller.load_from_json(pre_dt_preset(path));

    EXPECT_EQ(controller.track_name(2), "rhythm");
    EXPECT_EQ(controller.player().gain(2), 0.5f);
}

TEST_F(Medium_RigControllerClips, LoadingAPreDtPresetResetsTheTempoToOneTwentyBpmInFourFour)
{
    const std::string path =
        TestSupport::write_temp_float_wav("legacy.wav", TestSupport::make_test_signal(256), 48000);
    RigController controller = rig({48000.0, 64, 2});
    controller.musical_clock().set_tempo(90.0);
    controller.musical_clock().set_meter(3, 8);
    controller.metronome().set_enabled(true);

    controller.load_from_json(pre_dt_preset(path));

    EXPECT_EQ(controller.musical_clock().beats_per_minute(), 120.0);
    EXPECT_EQ(controller.musical_clock().beats_per_bar(), 4u);
    EXPECT_EQ(controller.musical_clock().beat_unit(), 4u);
    EXPECT_FALSE(controller.metronome().enabled());
}

