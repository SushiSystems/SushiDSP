/**************************************************************************/
/* test_session_file.cpp                                                  */
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
 * @file test_session_file.cpp
 * @brief Tests the project document, the new-project reset and a track's rig preset file.
 * @author Mustafa Garip
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "session.hpp"
#include "session_file.hpp"
#include "support/rig_products.hpp"
#include "support/rig_products_in_memory_files.hpp"

namespace
{
    /// @brief Holds what every session of the tests is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, 64, 2};

    /// @brief Holds the strip the document tests set on the second track.
    TrackStrip second_track_strip()
    {
        TrackStrip strip;
        strip.level_db = -3.0f;
        strip.pan = 0.25f;
        strip.muted = true;
        strip.rig_bypassed = true;
        return strip;
    }

    /// @brief Builds sessions on the catalogue of the fixture.
    class Small_SessionFile : public TestSupport::RigProducts
    {
        protected:
            /// @brief Writes a two-track session as a project document: a screamer on the first
            /// track, a delay and a screamer on the second, a strip and a preset path on the second.
            std::string two_track_document()
            {
                Session source(K_INFO, catalogue());
                std::string error;
                source.add_track(error);
                source.rig(0).append("sushidsp.tubescreamer");
                source.rig(1).append("sushidsp.dd3");
                source.rig(1).append("sushidsp.tubescreamer");
                source.set_strip(1, second_track_strip());
                source.set_preset_path(1, "lead.json");
                return session_to_json(source);
            }
    };

    /// @brief Holds a session of the fixture's catalogue, whose takes and preset files live in the
    /// fixture's in-memory files.
    class Small_SessionFileOnFiles : public TestSupport::RigProductsInMemoryFiles
    {
        protected:
            Small_SessionFileOnFiles() : session_(K_INFO, catalogue(), files())
            {
            }

            /// @brief Puts a preset file holding a rig of one delay in the text files and returns
            /// its path.
            std::string write_delay_preset()
            {
                RigController source = rig(K_INFO);
                source.append("sushidsp.dd3");
                const std::string path = "memory/delay_preset.json";
                text_files().add(path, source.save_rig_to_json());
                return path;
            }

            Session session_;
    };
} // namespace

TEST_F(Small_SessionFile, ADocumentReloadsEveryTracksRig)
{
    Session target(K_INFO, catalogue());

    session_from_json(target, two_track_document());

    ASSERT_EQ(target.track_count(), 2u);
    EXPECT_EQ(target.rig(0).slot_order().size(), 1u);
    ASSERT_EQ(target.rig(1).slot_order().size(), 2u);
    EXPECT_EQ(target.rig(1).descriptor_id(target.rig(1).slot_order()[0]), "sushidsp.dd3");
}

// A float crosses JSON as its double, which parses back to the same float.
TEST_F(Small_SessionFile, ADocumentReloadsEveryTracksStrip)
{
    Session target(K_INFO, catalogue());

    session_from_json(target, two_track_document());

    ASSERT_EQ(target.track_count(), 2u);
    EXPECT_EQ(target.strip(1).level_db, -3.0f);
    EXPECT_EQ(target.strip(1).pan, 0.25f);
    EXPECT_TRUE(target.strip(1).muted);
    EXPECT_TRUE(target.strip(1).rig_bypassed);
}

TEST_F(Small_SessionFile, ADocumentReloadsEveryTracksPresetPath)
{
    Session target(K_INFO, catalogue());

    session_from_json(target, two_track_document());

    ASSERT_EQ(target.track_count(), 2u);
    EXPECT_EQ(target.preset_path(1), "lead.json");
}

TEST_F(Small_SessionFile, AReloadedDocumentWritesTheSameDocumentAgain)
{
    const std::string saved = two_track_document();
    Session target(K_INFO, catalogue());
    session_from_json(target, saved);

    EXPECT_EQ(session_to_json(target), saved);
}

TEST_F(Small_SessionFile, ALoneRigPresetLoadsAsTheFirstTracksRig)
{
    RigController alone = rig(K_INFO);
    alone.append("sushidsp.tubescreamer");
    Session session(K_INFO, catalogue());

    session_from_json(session, alone.save_to_json());

    EXPECT_EQ(session.rig(0).slot_order().size(), 1u);
    EXPECT_FALSE(session.strip(0).rig_bypassed);
}

TEST_F(Small_SessionFile, ALoneRigPresetGivesEachOtherLaneATrackThatPassesItsAudioByAnEmptyRig)
{
    RigController alone = rig(K_INFO);
    alone.append("sushidsp.tubescreamer");
    Session session(K_INFO, catalogue());

    session_from_json(session, alone.save_to_json());

    EXPECT_EQ(session.track_count(), alone.track_count());
    ASSERT_GT(session.track_count(), 1u);
    EXPECT_TRUE(session.rig(1).slot_order().empty());
    EXPECT_TRUE(session.strip(1).rig_bypassed);
}

TEST_F(Small_SessionFile, NewProjectLeavesOneTrackAndSelectsIt)
{
    Session session(K_INFO, catalogue());
    std::string error;
    ASSERT_TRUE(session.add_track(error)) << error;
    session.select(1);

    new_project(session);

    EXPECT_EQ(session.track_count(), 1u);
    EXPECT_EQ(session.selected(), 0u);
}

TEST_F(Small_SessionFile, NewProjectEmptiesTheRigOfTheFirstTrack)
{
    Session session(K_INFO, catalogue());
    session.rig(0).append("sushidsp.tubescreamer");

    new_project(session);

    EXPECT_TRUE(session.rig(0).slot_order().empty());
}

TEST_F(Small_SessionFile, NewProjectResetsTheStripOfTheFirstTrack)
{
    Session session(K_INFO, catalogue());
    TrackStrip strip;
    strip.muted = true;
    session.set_strip(0, strip);

    new_project(session);

    EXPECT_FALSE(session.strip(0).muted);
}

TEST_F(Small_SessionFileOnFiles, NewProjectClearsTheClipsOfTheFirstTrack)
{
    const std::string wav = add_wav("new_project.wav", std::vector<float>(4800, 0.5f), 48000);
    std::string error;
    ASSERT_TRUE(session_.timeline().place_clip(0, wav, 0, error)) << error;
    ASSERT_EQ(session_.timeline().clip_count(0), 1u);

    new_project(session_);

    EXPECT_EQ(session_.timeline().clip_count(0), 0u);
}

TEST_F(Small_SessionFileOnFiles, SavingATrackRigWritesTheRigDocumentOfThatTrack)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    session_.rig(1).append("sushidsp.dd3");
    const std::string path = "memory/saved_rig.json";

    ASSERT_TRUE(save_track_rig(session_, 1, path, error, text_files())) << error;

    std::string written;
    ASSERT_TRUE(read_text_file(path, written, error, text_files())) << error;
    EXPECT_EQ(written, session_.rig(1).save_rig_to_json());
}

TEST_F(Small_SessionFileOnFiles, SavingATrackRigRemembersTheFileAsItsPresetPath)
{
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;
    const std::string path = "memory/remembered_rig.json";

    ASSERT_TRUE(save_track_rig(session_, 1, path, error, text_files())) << error;

    EXPECT_EQ(session_.preset_path(1), path);
}

TEST_F(Small_SessionFileOnFiles, LoadingATrackRigReplacesTheRigOfThatTrack)
{
    const std::string path = write_delay_preset();
    session_.rig(0).append("sushidsp.tubescreamer");
    std::string error;

    ASSERT_TRUE(load_track_rig(session_, 0, path, error, text_files())) << error;

    ASSERT_EQ(session_.rig(0).slot_order().size(), 1u);
    EXPECT_EQ(session_.rig(0).descriptor_id(session_.rig(0).slot_order()[0]), "sushidsp.dd3");
}

TEST_F(Small_SessionFileOnFiles, LoadingATrackRigRemembersTheFileAsItsPresetPath)
{
    const std::string path = write_delay_preset();
    std::string error;

    ASSERT_TRUE(load_track_rig(session_, 0, path, error, text_files())) << error;

    EXPECT_EQ(session_.preset_path(0), path);
}

TEST_F(Small_SessionFileOnFiles, LoadingATrackRigMovesTheRigRevision)
{
    const std::string path = write_delay_preset();
    const std::uint64_t before = session_.rig_revision();
    std::string error;

    ASSERT_TRUE(load_track_rig(session_, 0, path, error, text_files())) << error;

    EXPECT_NE(session_.rig_revision(), before);
}

TEST_F(Small_SessionFileOnFiles, LoadingATrackRigLeavesTheTrackCountAsItWas)
{
    const std::string path = write_delay_preset();
    std::string error;
    ASSERT_TRUE(session_.add_track(error)) << error;

    ASSERT_TRUE(load_track_rig(session_, 0, path, error, text_files())) << error;

    EXPECT_EQ(session_.track_count(), 2u);
}

TEST_F(Small_SessionFileOnFiles, LoadingAMissingFileIsRefusedWithAReason)
{
    std::string error;

    EXPECT_FALSE(load_track_rig(session_, 0, "memory/missing.json", error, text_files()));

    EXPECT_FALSE(error.empty());
}
