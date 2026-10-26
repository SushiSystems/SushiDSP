/**************************************************************************/
/* test_hrtf_set_loader.cpp                                               */
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
 * @file test_hrtf_set_loader.cpp
 * @brief Tests for HrtfSetLoader against the six-direction self-test fixture.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>

namespace
{
    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Returns the path of the fixture Task 2's --self-test writes.
    std::string fixture_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }

    /// @brief Reads a file whole into memory.
    std::vector<unsigned char> read_file(const std::string& path)
    {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        const std::streampos length = stream.tellg();
        std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
        return bytes;
    }

    /// @brief Writes bytes to a scratch path, so a test can corrupt a copy of the fixture.
    void write_file(const std::string& path, const std::vector<unsigned char>& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
} // namespace

TEST(Medium_HrtfSetLoader, HrtfSetLoaderReadsEveryFieldOfTheFixture)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    ASSERT_TRUE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;

    ASSERT_EQ(set.direction_count(), 6u);
    EXPECT_NEAR(set.direction(0).azimuth_rad, 0.0, 1e-6);
    EXPECT_NEAR(set.direction(1).azimuth_rad, K_PI / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(2).azimuth_rad, K_PI, 1e-6);
    EXPECT_NEAR(set.direction(3).azimuth_rad, 3.0 * K_PI / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(4).elevation_rad, K_PI / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(5).elevation_rad, -K_PI / 2.0, 1e-6);

    EXPECT_EQ(set.decoder_order(), 3u);
    EXPECT_EQ(set.decoder_filters().size(), 32u * 256u);
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderRefusesAMissingFileAndNamesThePath)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/nothing.hrtf";

    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(path, 48000.0, set, error));
    EXPECT_NE(error.find(path), std::string::npos);
    EXPECT_TRUE(set.empty());
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderRefusesTheWrongMagic)
{
    std::vector<unsigned char> bytes = read_file(fixture_path());
    bytes[0] = static_cast<unsigned char>(bytes[0] + 1u);
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_bad_magic.hrtf";
    write_file(path, bytes);

    SushiDSP::room::HrtfSet set;
    std::string error;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(path, 48000.0, set, error));
    EXPECT_TRUE(set.empty());

    std::remove(path.c_str());
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderRefusesARateMismatch)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 96000.0, set, error));
    EXPECT_NE(error.find("48000"), std::string::npos);
    EXPECT_NE(error.find("96000"), std::string::npos);
    EXPECT_TRUE(set.empty());
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderRefusesATruncatedFile)
{
    std::vector<unsigned char> bytes = read_file(fixture_path());
    bytes.resize(bytes.size() / 2);
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_truncated.hrtf";
    write_file(path, bytes);

    SushiDSP::room::HrtfSet set;
    std::string error;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(path, 48000.0, set, error));
    EXPECT_TRUE(set.empty());

    std::remove(path.c_str());
}

TEST(Small_HrtfSetLoaderLogging, AMissingFileWritesOneErrorNamingThePath)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/nothing_at_all.hrtf";

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(path, 48000.0, set, error));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find(path), std::string::npos) << lines[0].message;
}

TEST(Small_HrtfSetLoaderLogging, ARateMismatchWritesOneErrorNamingBothRates)
{
    SushiDSP::room::HrtfSet set;
    std::string error;

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 96000.0, set, error));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("96000"), std::string::npos) << lines[0].message;
}

TEST(Small_HrtfSetLoaderLogging, AFileThatLoadsWritesNothing)
{
    SushiDSP::room::HrtfSet set;
    std::string error;

    SushiDSP::LogCapture capture;
    EXPECT_TRUE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;

    EXPECT_TRUE(capture.lines().empty());
}
