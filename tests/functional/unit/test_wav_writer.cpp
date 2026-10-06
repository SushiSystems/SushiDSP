/**************************************************************************/
/* test_wav_writer.cpp                                                    */
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
 * @file test_wav_writer.cpp
 * @brief Tests that WavWriter writes files WavFile reads back.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/io/WavFile.hpp"
#include "SushiDSP/io/WavWriter.hpp"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    /// @brief Names a file in the test temporary directory and removes it when the case ends,
    /// so no run leaves its fixtures behind for the next one.
    class TempWavFile
    {
    public:
        /// @brief Builds the path from the temporary directory and the given name.
        /// @param name The file's name inside that directory.
        explicit TempWavFile(const std::string& name)
            : path_(std::string(::testing::TempDir()) + name)
        {
        }

        /// @brief Removes the file, ignoring a file that was never created.
        ~TempWavFile()
        {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }

        TempWavFile(const TempWavFile&) = delete;
        TempWavFile& operator=(const TempWavFile&) = delete;

        /// @brief Reports the file's full path.
        const std::string& path() const { return path_; }

    private:
        /// @brief Holds the file's full path.
        std::string path_;
    };

    std::vector<float> sine(std::size_t count, double frequency, double sample_rate)
    {
        std::vector<float> samples(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            samples[i] = static_cast<float>(
                0.5 * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sample_rate));
        }
        return samples;
    }
}

TEST(Unit_Io, WavWriterFloat32RoundTripsSampleForSample)
{
    const TempWavFile   fixture("sushidsp_wav_writer_float32.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> samples = sine(256, 440.0, 48000.0);

    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
        const float* channels[1] = { samples.data() };
        writer.append(channels, static_cast<std::uint32_t>(samples.size()));
        EXPECT_EQ(writer.frames_written(), samples.size());
    }

    double sample_rate = 0.0;
    const auto readback = SushiDSP::WavFile::load_mono(path, &sample_rate);

    ASSERT_EQ(readback.size(), samples.size());
    EXPECT_DOUBLE_EQ(sample_rate, 48000.0);
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_FLOAT_EQ(readback[i], samples[i]);
    }
}

TEST(Unit_Io, WavWriterPcm16RoundTripsWithinOneQuantisationStep)
{
    const TempWavFile   fixture("sushidsp_wav_writer_pcm16.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> samples = sine(256, 440.0, 48000.0);
    // The writer's 32767 scale and the reader's 32768 divide bound a sample of magnitude a at (0.5 + a) / 32768.
    const float peak_magnitude = 0.5f;
    const float tolerance = (0.5f + peak_magnitude) / 32768.0f;

    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Pcm16);
        const float* channels[1] = { samples.data() };
        writer.append(channels, static_cast<std::uint32_t>(samples.size()));
    }

    const auto readback = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(readback.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_NEAR(readback[i], samples[i], tolerance);
    }
}

TEST(Unit_Io, WavWriterTwoChannelsDownmixToTheMeanOnReadback)
{
    const TempWavFile   fixture("sushidsp_wav_writer_stereo.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> left = sine(128, 220.0, 48000.0);
    std::vector<float> right(left.size());
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        right[i] = -0.5f * left[i];
    }

    {
        SushiDSP::WavWriter writer(path, 48000.0, 2, SushiDSP::WavWriter::Format::Float32);
        const float* channels[2] = { left.data(), right.data() };
        writer.append(channels, static_cast<std::uint32_t>(left.size()));
    }

    const auto readback = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(readback.size(), left.size());
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        const float expected_mean = 0.5f * (left[i] + right[i]);
        EXPECT_NEAR(readback[i], expected_mean, 1e-6f);
    }
}

TEST(Unit_Io, WavWriterCloseTwiceIsSafeAndLeavesTheFileValid)
{
    const TempWavFile   fixture("sushidsp_wav_writer_double_close.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> samples = sine(64, 220.0, 48000.0);

    SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
    const float* channels[1] = { samples.data() };
    writer.append(channels, static_cast<std::uint32_t>(samples.size()));
    writer.close();
    writer.close();

    const auto readback = SushiDSP::WavFile::load_mono(path);
    ASSERT_EQ(readback.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_FLOAT_EQ(readback[i], samples[i]);
    }
}

TEST(Unit_Io, WavWriterHeaderSizesAreCorrectAfterSeveralAppendCalls)
{
    const TempWavFile   fixture("sushidsp_wav_writer_sizes.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> first_block = sine(100, 220.0, 48000.0);
    const std::vector<float> second_block = sine(37, 330.0, 48000.0);

    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Pcm16);
        const float* first_channels[1] = { first_block.data() };
        const float* second_channels[1] = { second_block.data() };
        writer.append(first_channels, static_cast<std::uint32_t>(first_block.size()));
        writer.append(second_channels, static_cast<std::uint32_t>(second_block.size()));
        EXPECT_EQ(writer.frames_written(), first_block.size() + second_block.size());
    }

    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(static_cast<bool>(file));
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    const auto read_u32_le = [&bytes](std::size_t offset) {
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    };

    const std::uint32_t expected_data_bytes =
        static_cast<std::uint32_t>((first_block.size() + second_block.size()) * sizeof(std::int16_t));
    EXPECT_EQ(read_u32_le(4), 36 + expected_data_bytes);
    EXPECT_EQ(read_u32_le(40), expected_data_bytes);
    EXPECT_EQ(bytes.size(), 44 + expected_data_bytes);
}

TEST(Unit_Io, WavWriterFloat32HeaderCarriesAnExtensibleFmtChunkAndAFactChunk)
{
    const TempWavFile   fixture("sushidsp_wav_writer_fact_chunk.wav");
    const std::string&  path = fixture.path();
    const std::vector<float> samples = sine(50, 440.0, 48000.0);

    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
        const float* channels[1] = { samples.data() };
        writer.append(channels, static_cast<std::uint32_t>(samples.size()));
    }

    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(static_cast<bool>(file));
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    const auto read_u32_le = [&bytes](std::size_t offset) {
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    };
    const auto read_u16_le = [&bytes](std::size_t offset) {
        return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
    };
    const auto read_tag = [&bytes](std::size_t offset) {
        return std::string(reinterpret_cast<const char*>(bytes.data() + offset), 4);
    };

    const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));

    EXPECT_EQ(read_tag(0), "RIFF");
    EXPECT_EQ(read_u32_le(4), 50u + data_bytes);
    EXPECT_EQ(read_tag(8), "WAVE");
    EXPECT_EQ(read_tag(12), "fmt ");
    EXPECT_EQ(read_u32_le(16), 18u);
    EXPECT_EQ(read_u16_le(20), 3u); // IEEE float format tag.
    EXPECT_EQ(read_u16_le(36), 0u); // cbSize.
    EXPECT_EQ(read_tag(38), "fact");
    EXPECT_EQ(read_u32_le(42), 4u);
    EXPECT_EQ(read_u32_le(46), samples.size());
    EXPECT_EQ(read_tag(50), "data");
    EXPECT_EQ(read_u32_le(54), data_bytes);
    EXPECT_EQ(bytes.size(), 58u + data_bytes);

    const auto readback = SushiDSP::WavFile::load_mono(path);
    ASSERT_EQ(readback.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_FLOAT_EQ(readback[i], samples[i]);
    }
}

TEST(Unit_Io, WavWriterThrowsWhenThePathCannotBeOpened)
{
    const TempWavFile   fixture("sushidsp_wav_writer_no_such_dir/take.wav");
    const std::string&  path = fixture.path();

    EXPECT_THROW(
        SushiDSP::WavWriter(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32),
        std::runtime_error);
}
