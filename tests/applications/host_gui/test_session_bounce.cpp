/**************************************************************************/
/* test_session_bounce.cpp                                                */
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
 * @file test_session_bounce.cpp
 * @brief Tests the offline export: what it renders into a WAV file and when it refuses a request.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/nodes/capture/take.hpp>
#include "session.hpp"
#include "session_bounce.hpp"
#include "session_file.hpp"
#include "support/rig_products.hpp"
#include "support/rig_products_in_memory_files.hpp"
#include "support/tolerance.hpp"

namespace
{
    /// @brief Holds the block the export renders in.
    constexpr std::uint32_t K_BLOCK = 64;

    /// @brief Holds what every session of the tests is prepared at.
    const SushiDSP::PrepareInfo K_INFO{48000.0, K_BLOCK, 2};

    /// @brief Holds the length of the clip every export renders, in frames; 75 whole blocks.
    constexpr std::size_t K_CLIP_FRAMES = 4800;

    /// @brief Holds the level of the clip's samples.
    constexpr float K_CLIP_LEVEL = 0.5f;

    /// @brief Holds the fader of the exported track, in decibels: 20 log10(2) rounded to four places.
    constexpr float K_FADER_DB = -6.0206f;

    /// @brief Holds the frames of silence that end the tail: 0.5 s at 48 kHz is 24000 frames, which
    /// is 375 blocks of 64, and the tail ends on the block after them.
    constexpr std::size_t K_TAIL_FRAMES = 376 * K_BLOCK;

    /// @brief Reads a take back as one vector of samples per channel.
    std::vector<std::vector<float>> channels_of(const SushiDSP::Take& take)
    {
        std::vector<std::vector<float>> channels;
        for (std::uint32_t channel = 0; channel < take.channels(); ++channel)
        {
            channels.emplace_back(take.channel(channel), take.channel(channel) + take.frames());
        }
        return channels;
    }

    /// @brief Builds sessions on the catalogue of the fixture; the clip and the export live in its
    /// in-memory files.
    class SessionBounceCase : public TestSupport::RigProductsInMemoryFiles
    {
        protected:
            /// @brief Names a file a case writes.
            std::string track_path(const std::string& name)
            {
                return "memory/" + name;
            }

            /// @brief Builds the request that exports the one-clip session to @p path.
            /// @param format The file's kind.
            BounceRequest request_for_the_clip(const std::string& path, BounceFormat format)
            {
                const std::string wav = add_wav(
                    "bounce_source.wav", std::vector<float>(K_CLIP_FRAMES, K_CLIP_LEVEL), 48000);
                Session session(K_INFO, catalogue(), files());
                std::string error;
                session.timeline().place_clip(0, wav, 0, error);
                TrackStrip strip;
                strip.level_db = K_FADER_DB;
                session.set_strip(0, strip);

                BounceRequest request;
                request.session_json = session_to_json(session);
                request.path = path;
                request.format = format;
                request.info = K_INFO;
                request.catalogue = &catalogue();
                request.files = &files();
                return request;
            }

            /// @brief Exports the one-clip session as a 32-bit float WAV and reads the file back; a failed
            /// export leaves no readable file, which fails the case that reads it.
            std::vector<std::vector<float>> bounce_the_clip()
            {
                const std::string path = track_path("bounce_out.wav");
                SessionBounce::render(request_for_the_clip(path, BounceFormat::Wav32Float), nullptr);
                return channels_of(files().read(path));
            }
    };

    class Small_SessionBounce : public SessionBounceCase
    {
    };

    /// @brief Builds sessions on the catalogue of the fixture and writes no file.
    class Small_SessionBounceRefusal : public TestSupport::RigProducts
    {
    };
} // namespace

// The strip alone scales the constant clip; the three roundings are derived in the README.
TEST_F(Small_SessionBounce, TheExportCarriesTheClipThroughTheStripGainOnBothChannels)
{
    const std::vector<std::vector<float>> channels = bounce_the_clip();

    ASSERT_EQ(channels.size(), 2u);
    ASSERT_GT(channels[0].size(), 2400u);
    const double expected = K_CLIP_LEVEL * std::pow(10.0, static_cast<double>(K_FADER_DB) / 20.0);
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(channels[0][2400], expected, 3)));
    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(channels[1][2400], expected, 3)));
}

// The length of the tail is derived in the README: 376 silent blocks of 64 frames.
TEST_F(Small_SessionBounce, TheExportEndsHalfASecondOfSilenceAfterTheClip)
{
    const std::vector<std::vector<float>> channels = bounce_the_clip();

    ASSERT_EQ(channels.size(), 2u);
    EXPECT_EQ(channels[0].size(), K_CLIP_FRAMES + K_TAIL_FRAMES);
    EXPECT_EQ(channels[1].size(), K_CLIP_FRAMES + K_TAIL_FRAMES);
}

TEST_F(Small_SessionBounce, TheExportIsSilentAfterTheClip)
{
    const std::vector<std::vector<float>> channels = bounce_the_clip();

    ASSERT_EQ(channels.size(), 2u);
    ASSERT_GT(channels[0].size(), K_CLIP_FRAMES + 1200u);
    EXPECT_EQ(channels[0][K_CLIP_FRAMES + 1200u], 0.0f);
    EXPECT_EQ(channels[1][K_CLIP_FRAMES + 1200u], 0.0f);
}

// The test target compiles session_bounce.cpp without SUSHIDSP_HAVE_LAME, so an MP3 is refused.
TEST_F(Small_SessionBounce, AnMp3RequestIsRefusedWhenTheBuildHasNoEncoder)
{
    const BounceRequest request =
        request_for_the_clip(track_path("bounce_out.mp3"), BounceFormat::Mp3);

    EXPECT_FALSE(SessionBounce::render(request, nullptr).empty());
}

TEST_F(Small_SessionBounceRefusal, ASessionWithNoClipIsRefused)
{
    Session session(K_INFO, catalogue());
    BounceRequest request;
    request.session_json = session_to_json(session);
    request.path = "bounce_none.wav";
    request.info = K_INFO;
    request.catalogue = &catalogue();

    EXPECT_FALSE(SessionBounce::render(request, nullptr).empty());
}
