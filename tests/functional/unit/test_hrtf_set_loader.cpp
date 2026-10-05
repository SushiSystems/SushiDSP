/****************************************************************/
/* test_hrtf_set_loader.cpp                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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

#include "SushiDSP/room/HrtfSet.hpp"
#include "SushiDSP/room/HrtfSetLoader.hpp"

namespace
{
    /// @brief Holds pi.
    constexpr double kPi = 3.14159265358979323846;

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
}

TEST(Unit_Dsp, HrtfSetLoaderReadsEveryFieldOfTheFixture)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    ASSERT_TRUE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 48000.0, set, error))
        << error;

    ASSERT_EQ(set.direction_count(), 6u);
    EXPECT_NEAR(set.direction(0).azimuth_rad, 0.0, 1e-6);
    EXPECT_NEAR(set.direction(1).azimuth_rad, kPi / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(2).azimuth_rad, kPi, 1e-6);
    EXPECT_NEAR(set.direction(3).azimuth_rad, 3.0 * kPi / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(4).elevation_rad, kPi / 2.0, 1e-6);
    EXPECT_NEAR(set.direction(5).elevation_rad, -kPi / 2.0, 1e-6);

    EXPECT_EQ(set.decoder_order(), 3u);
    EXPECT_EQ(set.decoder_filters().size(), 32u * 256u);
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesAMissingFileAndNamesThePath)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/nothing.hrtf";

    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(path, 48000.0, set, error));
    EXPECT_NE(error.find(path), std::string::npos);
    EXPECT_TRUE(set.empty());
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesTheWrongMagic)
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

TEST(Unit_Dsp, HrtfSetLoaderRefusesARateMismatch)
{
    SushiDSP::room::HrtfSet set;
    std::string error;
    EXPECT_FALSE(SushiDSP::room::HrtfSetLoader::load(fixture_path(), 96000.0, set, error));
    EXPECT_NE(error.find("48000"), std::string::npos);
    EXPECT_NE(error.find("96000"), std::string::npos);
    EXPECT_TRUE(set.empty());
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesATruncatedFile)
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
