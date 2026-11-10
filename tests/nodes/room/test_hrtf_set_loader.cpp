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
 * @file test_hrtf_set_loader.cpp
 * @brief Tests for HrtfSetLoader: the fixture file through load, and in-memory assets through decode.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/nodes/room/hrtf_set.hpp>
#include <SushiDSP/nodes/room/hrtf_set_loader.hpp>

#include "support/tolerance.hpp"

namespace
{
    using SushiDSP::room::HrtfSet;
    using SushiDSP::room::HrtfSetLoader;
    using Bytes = std::vector<unsigned char>;

    /// @brief Holds pi.
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief Counts the directions of the asset: the six axes.
    constexpr std::uint32_t K_DIRECTIONS = 6;

    /// @brief Holds the decoder order the asset carries: third order, sixteen channels.
    constexpr std::uint32_t K_ORDER = 3;

    /// @brief Counts the ambisonic channels of that order.
    constexpr std::uint32_t K_CHANNELS = (K_ORDER + 1) * (K_ORDER + 1);

    /// @brief Holds the rate the asset is made at.
    constexpr std::uint32_t K_ASSET_RATE_HZ = 48000;

    /// @brief Holds the rate a mismatch test asks for.
    constexpr double K_OTHER_RATE_HZ = 96000.0;

    /// @brief Names the in-memory asset in every error.
    const std::string K_SOURCE = "in_memory.hrtf";

    /// @brief Appends a little-endian uint32_t.
    void put_u32(Bytes& bytes, std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xFFu));
        }
    }

    /// @brief Appends a little-endian float.
    void put_f32(Bytes& bytes, float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        put_u32(bytes, bits);
    }

    /// @brief Appends count floats of zero.
    void put_zeros(Bytes& bytes, std::size_t count)
    {
        bytes.insert(bytes.end(), count * sizeof(float), 0);
    }

    /// @brief Returns an asset of the six axes at the given rate, laid out as convert_hrtf.py writes it.
    Bytes valid_asset(std::uint32_t rate_hz)
    {
        const std::array<std::array<float, 2>, K_DIRECTIONS> axes = {{{0.0f, 0.0f},
                                                                      {static_cast<float>(K_PI / 2.0), 0.0f},
                                                                      {0.0f, static_cast<float>(K_PI / 2.0)},
                                                                      {static_cast<float>(K_PI), 0.0f},
                                                                      {-static_cast<float>(K_PI / 2.0), 0.0f},
                                                                      {0.0f, -static_cast<float>(K_PI / 2.0)}}};
        Bytes bytes = {'S', 'D', 'H', 'R', 'T', 'F', 0, 0};
        put_u32(bytes, 1);
        put_u32(bytes, rate_hz);
        put_u32(bytes, K_DIRECTIONS);
        put_u32(bytes, HrtfSet::K_TAP_COUNT);
        put_u32(bytes, K_ORDER);
        put_u32(bytes, 0);
        for (const std::array<float, 2>& axis : axes)
        {
            put_f32(bytes, axis[0]);
            put_f32(bytes, axis[1]);
            put_f32(bytes, 0.0f);
            put_zeros(bytes, 2 * HrtfSet::K_TAP_COUNT);
        }
        put_zeros(bytes, 2 * K_CHANNELS * HrtfSet::K_DECODER_TAPS);
        put_zeros(bytes, 2 * HrtfSet::K_DIFFUSE_TAPS);
        return bytes;
    }

    /// @brief Returns the path of the fixture Task 2's --self-test writes.
    std::string fixture_path()
    {
        return std::string(SUSHIDSP_FIXTURE_DIR) + "/hrtf_self_test.hrtf";
    }

    /// @brief Holds one angle of the fixture's six directions and the value it was written from.
    struct FixtureAngle
    {
            const char* name;
            std::uint32_t direction;
            bool is_azimuth;
            double radians;
    };

    const std::array<FixtureAngle, 6> kFixtureAngles = {{
        {"Direction0Azimuth", 0, true, 0.0},
        {"Direction1Azimuth", 1, true, K_PI / 2.0},
        {"Direction2Azimuth", 2, true, K_PI},
        {"Direction3Azimuth", 3, true, 3.0 * K_PI / 2.0},
        {"Direction4Elevation", 4, false, K_PI / 2.0},
        {"Direction5Elevation", 5, false, -K_PI / 2.0},
    }};

    /// @brief Names a fixture angle case after the angle.
    std::string angle_name(const ::testing::TestParamInfo<FixtureAngle>& info)
    {
        return info.param.name;
    }
} // namespace

class Medium_HrtfSetLoaderFixtureAngles : public ::testing::TestWithParam<FixtureAngle>
{
};

TEST_P(Medium_HrtfSetLoaderFixtureAngles, HrtfSetLoaderReadsTheAngleOfTheFixture)
{
    HrtfSet set;
    std::string error;
    ASSERT_TRUE(HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;
    const HrtfSet::Direction& direction = set.direction(GetParam().direction);
    const double stored = GetParam().is_azimuth ? direction.azimuth_rad : direction.elevation_rad;

    // The file holds the angle rounded once to float, so the miss is one roundoff of the angle.
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(stored, GetParam().radians, 1)));
}

INSTANTIATE_TEST_SUITE_P(Angles, Medium_HrtfSetLoaderFixtureAngles, ::testing::ValuesIn(kFixtureAngles), angle_name);

TEST(Medium_HrtfSetLoader, HrtfSetLoaderReadsTheDirectionCountOfTheFixture)
{
    HrtfSet set;
    std::string error;
    ASSERT_TRUE(HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;

    EXPECT_EQ(set.direction_count(), K_DIRECTIONS);
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderReadsTheDecoderOrderOfTheFixture)
{
    HrtfSet set;
    std::string error;
    ASSERT_TRUE(HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;

    EXPECT_EQ(set.decoder_order(), K_ORDER);
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderReadsTheDecoderFilterLengthOfTheFixture)
{
    HrtfSet set;
    std::string error;
    ASSERT_TRUE(HrtfSetLoader::load(fixture_path(), 48000.0, set, error)) << error;

    EXPECT_EQ(set.decoder_filters().size(), 2u * K_CHANNELS * HrtfSet::K_DECODER_TAPS);
}

TEST(Medium_HrtfSetLoader, HrtfSetLoaderRefusesAMissingFileAndNamesThePath)
{
    HrtfSet set;
    std::string error;
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/nothing.hrtf";

    EXPECT_FALSE(HrtfSetLoader::load(path, 48000.0, set, error));
    EXPECT_NE(error.find(path), std::string::npos);
    EXPECT_TRUE(set.empty());
}

TEST(Small_HrtfSetLoader, HrtfSetLoaderDecodesAValidAsset)
{
    HrtfSet set;
    std::string error;

    EXPECT_TRUE(HrtfSetLoader::decode(valid_asset(K_ASSET_RATE_HZ), K_SOURCE, 48000.0, set, error)) << error;
    EXPECT_EQ(set.direction_count(), K_DIRECTIONS);
}

TEST(Small_HrtfSetLoader, HrtfSetLoaderRefusesTheWrongMagic)
{
    Bytes bytes = valid_asset(K_ASSET_RATE_HZ);
    bytes[0] = static_cast<unsigned char>(bytes[0] + 1u);
    HrtfSet set;
    std::string error;

    EXPECT_FALSE(HrtfSetLoader::decode(bytes, K_SOURCE, 48000.0, set, error));
    EXPECT_TRUE(set.empty());
    EXPECT_NE(error.find("magic"), std::string::npos);
}

TEST(Small_HrtfSetLoader, HrtfSetLoaderRefusesARateMismatch)
{
    HrtfSet set;
    std::string error;

    EXPECT_FALSE(HrtfSetLoader::decode(valid_asset(K_ASSET_RATE_HZ), K_SOURCE, K_OTHER_RATE_HZ, set, error));
    EXPECT_TRUE(set.empty());
    EXPECT_NE(error.find("48000"), std::string::npos);
    EXPECT_NE(error.find("96000"), std::string::npos);
}

TEST(Small_HrtfSetLoader, HrtfSetLoaderRefusesATruncatedFile)
{
    Bytes bytes = valid_asset(K_ASSET_RATE_HZ);
    bytes.resize(bytes.size() / 2);
    HrtfSet set;
    std::string error;

    EXPECT_FALSE(HrtfSetLoader::decode(bytes, K_SOURCE, 48000.0, set, error));
    EXPECT_TRUE(set.empty());
    EXPECT_NE(error.find("truncated"), std::string::npos);
}

TEST(Small_HrtfSetLoader, HrtfSetLoaderRefusesBytesShorterThanItsHeader)
{
    HrtfSet set;
    std::string error;

    EXPECT_FALSE(HrtfSetLoader::decode(Bytes(8, 0), K_SOURCE, 48000.0, set, error));
    EXPECT_NE(error.find("too short"), std::string::npos);
}

TEST(Medium_HrtfSetLoaderLogging, AMissingFileWritesOneErrorNamingThePath)
{
    HrtfSet set;
    std::string error;
    const std::string path = std::string(SUSHIDSP_FIXTURE_DIR) + "/nothing_at_all.hrtf";

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(HrtfSetLoader::load(path, 48000.0, set, error));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Error);
    EXPECT_EQ(lines[0].category, "room");
    EXPECT_NE(lines[0].message.find(path), std::string::npos) << lines[0].message;
}

TEST(Small_HrtfSetLoaderLogging, ARateMismatchWritesOneErrorNamingBothRates)
{
    HrtfSet set;
    std::string error;

    SushiDSP::LogCapture capture;
    EXPECT_FALSE(HrtfSetLoader::decode(valid_asset(K_ASSET_RATE_HZ), K_SOURCE, K_OTHER_RATE_HZ, set, error));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].message.find("96000"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("48000"), std::string::npos) << lines[0].message;
}

TEST(Small_HrtfSetLoaderLogging, AnAssetThatLoadsWritesNothing)
{
    HrtfSet set;
    std::string error;

    SushiDSP::LogCapture capture;
    EXPECT_TRUE(HrtfSetLoader::decode(valid_asset(K_ASSET_RATE_HZ), K_SOURCE, 48000.0, set, error)) << error;

    EXPECT_TRUE(capture.lines().empty());
}
