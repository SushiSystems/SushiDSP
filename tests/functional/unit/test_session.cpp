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
 * @file test_session.cpp
 * @brief Tests the session's tracks, its block renderer, its documents and its export.
 * @author Mustafa Garip & Sushi Systems
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/capture/take.hpp"
#include "SushiDSP/io/wav_file.hpp"
#include "SushiDSP/io/wav_writer.hpp"
#include "session.hpp"
#include "session_bounce.hpp"
#include "session_file.hpp"
#include "session_renderer.hpp"

namespace
{
    /// @brief Holds the block every test renders in.
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds what every session of the tests is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, K_BLOCK, 2};

    /// @brief Returns a mono take holding one value in every frame.
    std::unique_ptr<SushiDSP::Take> constant_take(std::size_t frames, float value)
    {
        auto take = std::make_unique<SushiDSP::Take>(1u, 48000.0, frames);
        float* samples = take->mutable_channel(0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            samples[frame] = value;
        }
        return take;
    }

    /// @brief Renders one block of a session and returns its two channels.
    std::array<std::vector<float>, 2> render_block(Session& session, SessionRenderer& renderer,
                                                   const float* live_channel)
    {
        std::array<std::vector<float>, 2> out{std::vector<float>(K_BLOCK, 9.0f),
                                              std::vector<float>(K_BLOCK, 9.0f)};
        float* outputs[2] = {out[0].data(), out[1].data()};
        const float* live[2] = {live_channel, live_channel};
        renderer.render(session, live_channel != nullptr ? live : nullptr, outputs, K_BLOCK);
        return out;
    }

    /// @brief Writes a mono WAV of one value and returns its path.
    std::string write_constant_wav(const std::string& name, std::size_t frames, float value)
    {
        const std::string path = (std::filesystem::temp_directory_path() / name).string();
        SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
        const std::vector<float> samples(frames, value);
        const float* channels[1] = {samples.data()};
        writer.append(channels, static_cast<std::uint32_t>(frames));
        writer.close();
        return path;
    }
} // namespace

TEST(Unit_Dsp, StripGainsAreAFaderABalanceAndSilenceUnderMuteOrAnotherSolo)
{
    TrackStrip strip;
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[0], 1.0f);
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[1], 1.0f);

    strip.level_db = -6.0206f;
    EXPECT_NEAR(strip_gains(strip, false)[0], 0.5f, 1e-4f);

    strip.level_db = 0.0f;
    strip.pan = 0.5f;
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[0], 0.5f);
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[1], 1.0f);
    strip.pan = -1.0f;
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[1], 0.0f);

    strip.pan = 0.0f;
    strip.muted = true;
    EXPECT_FLOAT_EQ(strip_gains(strip, false)[0], 0.0f);
    // A solo elsewhere silences a track that is not soloed, and a soloed track ignores its mute.
    strip.muted = false;
    EXPECT_FLOAT_EQ(strip_gains(strip, true)[0], 0.0f);
    strip.soloed = true;
    strip.muted = true;
    EXPECT_FLOAT_EQ(strip_gains(strip, true)[0], 1.0f);
}

TEST(Unit_Dsp, SessionStartsWithOneTrackAndGivesEachTrackARigOfItsOwn)
{
    Session session(K_INFO);
    EXPECT_EQ(session.track_count(), 1u);
    EXPECT_EQ(session.timeline().track_count(), 1u);
    EXPECT_EQ(session.selected(), 0u);

    std::string error;
    ASSERT_TRUE(session.add_track(error)) << error;
    ASSERT_TRUE(session.add_track(error)) << error;
    EXPECT_EQ(session.track_count(), 3u);
    EXPECT_NE(&session.rig(0), &session.rig(1));
    // Every rig reads the one timeline.
    EXPECT_EQ(session.rig(1).track_count(), 3u);
    EXPECT_EQ(session.audio().tracks[2].rig.load(), &session.rig(2));
    EXPECT_EQ(session.audio().tracks[3].rig.load(), nullptr);

    session.rig(1).append("sushidsp.tubescreamer");
    EXPECT_EQ(session.rig(1).slot_order().size(), 1u);
    EXPECT_TRUE(session.rig(0).slot_order().empty());
}

TEST(Unit_Dsp, SessionRemovesATrackMovesTheOthersDownAndKeepsTheLast)
{
    Session session(K_INFO);
    std::string error;
    ASSERT_TRUE(session.add_track(error));
    ASSERT_TRUE(session.add_track(error));
    RigController* const third = &session.rig(2);
    session.select(2);
    const std::uint64_t before = session.rig_revision();

    ASSERT_TRUE(session.remove_track(1, false, error)) << error;
    EXPECT_EQ(session.track_count(), 2u);
    EXPECT_EQ(&session.rig(1), third);
    EXPECT_EQ(session.selected(), 1u);
    EXPECT_NE(session.rig_revision(), before);
    EXPECT_EQ(session.audio().tracks[1].rig.load(), third);
    EXPECT_EQ(session.audio().tracks[2].rig.load(), nullptr);

    ASSERT_TRUE(session.remove_track(1, false, error));
    EXPECT_FALSE(session.remove_track(0, false, error));
    EXPECT_EQ(session.track_count(), 1u);
    EXPECT_FALSE(session.remove_track(5, false, error));
}

TEST(Unit_Dsp, SessionRefusesANinthTrack)
{
    Session session(K_INFO);
    std::string error;
    for (std::size_t added = 1; added < Session::K_MAX_TRACKS; ++added)
    {
        ASSERT_TRUE(session.add_track(error)) << error;
    }
    EXPECT_FALSE(session.add_track(error));
    EXPECT_EQ(session.track_count(), Session::K_MAX_TRACKS);
}

TEST(Unit_Dsp, SessionRendererHearsTheLiveInputOnTheSelectedTrackWhileStopped)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    const std::vector<float> live(K_BLOCK, 0.25f);

    // An empty rig passes its input, and a stopped transport plays no lane.
    session.timeline().player().set_take(0, constant_take(4096, 0.5f));
    std::array<std::vector<float>, 2> out = render_block(session, renderer, live.data());
    EXPECT_FLOAT_EQ(out[0][10], 0.25f);
    EXPECT_FLOAT_EQ(out[1][10], 0.25f);

    out = render_block(session, renderer, nullptr);
    EXPECT_FLOAT_EQ(out[0][10], 0.0f);
    EXPECT_EQ(session.audio().blocks.load(), 2u);
}

TEST(Unit_Dsp, SessionRendererSumsEveryTracksLaneThroughItsStripWhileRolling)
{
    Session session(K_INFO);
    std::string error;
    ASSERT_TRUE(session.add_track(error));
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    session.timeline().player().set_take(0, constant_take(4096, 0.5f));
    session.timeline().player().set_take(1, constant_take(4096, 0.125f));
    TrackStrip second;
    second.pan = -1.0f;
    session.set_strip(1, second);
    session.select(1);
    session.timeline().transport().play();

    const std::vector<float> live(K_BLOCK, 0.25f);
    std::array<std::vector<float>, 2> out = render_block(session, renderer, live.data());
    // Left: lane 0, and lane 1 with the live input; right: lane 0 alone, lane 1 turned left.
    EXPECT_FLOAT_EQ(out[0][10], 0.5f + 0.125f + 0.25f);
    EXPECT_FLOAT_EQ(out[1][10], 0.5f);
    EXPECT_EQ(session.timeline().transport().position_frames(), K_BLOCK);

    TrackStrip first;
    first.soloed = true;
    session.set_strip(0, first);
    out = render_block(session, renderer, nullptr);
    EXPECT_FLOAT_EQ(out[0][10], 0.5f);
}

TEST(Unit_Dsp, SessionRendererPassesALaneByABypassedRig)
{
    Session session(K_INFO);
    SessionRenderer renderer;
    renderer.prepare(K_BLOCK);
    // A tube screamer changes a constant; the bypass must leave it as it is.
    session.rig(0).append("sushidsp.tubescreamer");
    session.timeline().player().set_take(0, constant_take(8192, 0.5f));
    session.timeline().transport().play();

    TrackStrip strip;
    strip.rig_bypassed = true;
    session.set_strip(0, strip);
    const std::array<std::vector<float>, 2> bypassed = render_block(session, renderer, nullptr);
    EXPECT_FLOAT_EQ(bypassed[0][40], 0.5f);

    strip.rig_bypassed = false;
    session.set_strip(0, strip);
    std::array<std::vector<float>, 2> through;
    for (int block = 0; block < 8; ++block)
    {
        through = render_block(session, renderer, nullptr);
    }
    EXPECT_NE(through[0][40], 0.5f);
}

TEST(Unit_Dsp, SessionDocumentRoundTripsEveryTracksRigAndStrip)
{
    Session source(K_INFO);
    std::string error;
    ASSERT_TRUE(source.add_track(error));
    source.rig(0).append("sushidsp.tubescreamer");
    source.rig(1).append("sushidsp.dd3");
    source.rig(1).append("sushidsp.tubescreamer");
    TrackStrip strip;
    strip.level_db = -3.0f;
    strip.pan = 0.25f;
    strip.muted = true;
    strip.rig_bypassed = true;
    source.set_strip(1, strip);
    source.set_preset_path(1, "lead.json");

    const std::string saved = session_to_json(source);
    Session target(K_INFO);
    session_from_json(target, saved);

    ASSERT_EQ(target.track_count(), 2u);
    EXPECT_EQ(target.rig(0).slot_order().size(), 1u);
    ASSERT_EQ(target.rig(1).slot_order().size(), 2u);
    EXPECT_EQ(target.rig(1).descriptor_id(target.rig(1).slot_order()[0]), "sushidsp.dd3");
    EXPECT_FLOAT_EQ(target.strip(1).level_db, -3.0f);
    EXPECT_FLOAT_EQ(target.strip(1).pan, 0.25f);
    EXPECT_TRUE(target.strip(1).muted);
    EXPECT_TRUE(target.strip(1).rig_bypassed);
    EXPECT_EQ(target.preset_path(1), "lead.json");
    EXPECT_EQ(session_to_json(target), saved);
}

TEST(Unit_Dsp, SessionLoadsAOneRigPresetAsATrackWithItsLanesBesideIt)
{
    RigController alone(K_INFO);
    alone.append("sushidsp.tubescreamer");
    const std::string preset = alone.save_to_json();

    Session session(K_INFO);
    session_from_json(session, preset);
    // The lone controller's timeline carries every lane it can; each becomes a track.
    EXPECT_EQ(session.track_count(), alone.track_count());
    EXPECT_EQ(session.rig(0).slot_order().size(), 1u);
    EXPECT_FALSE(session.strip(0).rig_bypassed);
    ASSERT_GT(session.track_count(), 1u);
    EXPECT_TRUE(session.rig(1).slot_order().empty());
    EXPECT_TRUE(session.strip(1).rig_bypassed);
}

TEST(Unit_Dsp, RigControllerLoadsARigAloneAndLeavesTheTimelineAsItIs)
{
    RigController source(K_INFO);
    source.append("sushidsp.dd3");
    const std::string rig_only = source.save_rig_to_json();
    EXPECT_EQ(rig_only.find("\"tracks\""), std::string::npos);
    EXPECT_EQ(rig_only.find("\"tempo\""), std::string::npos);

    Session session(K_INFO);
    std::string error;
    ASSERT_TRUE(session.add_track(error));
    session.timeline().musical_clock().set_tempo(97.0);
    session.rig(1).load_rig_from_json(rig_only);
    EXPECT_EQ(session.rig(1).slot_order().size(), 1u);
    EXPECT_EQ(session.track_count(), 2u);
    EXPECT_DOUBLE_EQ(session.timeline().musical_clock().beats_per_minute(), 97.0);
}

TEST(Unit_Dsp, TrackDocumentRoundTripsARigAStripAndItsClips)
{
    const std::string wav = write_constant_wav("sushidsp_track_document.wav", 4800, 0.5f);
    Session source(K_INFO);
    std::string error;
    source.rig(0).append("sushidsp.tubescreamer");
    ASSERT_TRUE(source.timeline().place_clip(0, wav, 9600, error)) << error;
    source.timeline().set_track_name(0, "Rhythm");
    TrackStrip strip;
    strip.level_db = -9.0f;
    source.set_strip(0, strip);

    const std::string document = track_to_json(source, 0);
    Session target(K_INFO);
    ASSERT_TRUE(track_from_json(target, document, error)) << error;
    EXPECT_TRUE(error.empty()) << error;

    ASSERT_EQ(target.track_count(), 2u);
    EXPECT_EQ(target.selected(), 1u);
    EXPECT_EQ(target.timeline().track_name(1), "Rhythm");
    EXPECT_FLOAT_EQ(target.strip(1).level_db, -9.0f);
    EXPECT_EQ(target.rig(1).slot_order().size(), 1u);
    ASSERT_EQ(target.timeline().clip_count(1), 1u);
    EXPECT_EQ(target.timeline().clip(1, 0).start_frame, 9600u);

    EXPECT_FALSE(track_from_json(target, "{\"kind\":\"something else\"}", error));
    EXPECT_EQ(target.track_count(), 2u);
    std::filesystem::remove(wav);
}

TEST(Unit_Dsp, SessionBounceWritesTheRenderedMixToAWav)
{
    const std::string wav = write_constant_wav("sushidsp_bounce_source.wav", 4800, 0.5f);
    const std::string out =
        (std::filesystem::temp_directory_path() / "sushidsp_bounce_out.wav").string();
    Session session(K_INFO);
    std::string error;
    ASSERT_TRUE(session.timeline().place_clip(0, wav, 0, error)) << error;
    TrackStrip strip;
    strip.level_db = -6.0206f;
    session.set_strip(0, strip);

    BounceRequest request;
    request.session_json = session_to_json(session);
    request.path = out;
    request.format = BounceFormat::Wav32Float;
    request.info = K_INFO;
    ASSERT_EQ(SessionBounce::render(request, nullptr), "");

    const std::vector<std::vector<float>> channels = SushiDSP::WavFile::load_channels(out);
    ASSERT_EQ(channels.size(), 2u);
    // The clip, then the half second of silence that ends the tail.
    EXPECT_GE(channels[0].size(), 4800u + 24000u);
    EXPECT_LT(channels[0].size(), 4800u + 2u * 24000u);
    EXPECT_NEAR(channels[0][2400], 0.25f, 1e-3f);
    EXPECT_NEAR(channels[1][2400], 0.25f, 1e-3f);
    EXPECT_FLOAT_EQ(channels[0][6000], 0.0f);

    request.format = BounceFormat::Mp3;
    EXPECT_NE(SessionBounce::render(request, nullptr), "");
    std::filesystem::remove(wav);
    std::filesystem::remove(out);
}

TEST(Unit_Dsp, SessionBounceRefusesASessionWithNoClip)
{
    Session session(K_INFO);
    BounceRequest request;
    request.session_json = session_to_json(session);
    request.path = (std::filesystem::temp_directory_path() / "sushidsp_bounce_none.wav").string();
    request.info = K_INFO;
    EXPECT_NE(SessionBounce::render(request, nullptr), "");
}
