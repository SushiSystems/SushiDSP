/**************************************************************************/
/* test_wav_file.cpp                                                      */
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
 * @file test_wav_file.cpp
 * @brief Tests the loading of PCM and IEEE float WAV files in WavFile.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/wav_file.hpp>

namespace
{
    void write_u32_le(std::vector<uint8_t>& buffer, uint32_t value)
    {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    void write_u16_le(std::vector<uint8_t>& buffer, uint16_t value)
    {
        buffer.push_back(static_cast<uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void write_tag(std::vector<uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    // Builds a minimal mono 16-bit PCM WAV file with the given sample_rate and int16 samples.
    std::vector<uint8_t> build_pcm16_wav(uint32_t sample_rate, const std::vector<int16_t>& samples)
    {
        const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
        const uint32_t byte_rate = sample_rate * 1 * 16 / 8;

        std::vector<uint8_t> buffer;
        write_tag(buffer, "RIFF");
        write_u32_le(buffer, 36 + data_bytes);
        write_tag(buffer, "WAVE");

        write_tag(buffer, "fmt ");
        write_u32_le(buffer, 16);
        write_u16_le(buffer, 1); // PCM
        write_u16_le(buffer, 1); // mono
        write_u32_le(buffer, sample_rate);
        write_u32_le(buffer, byte_rate);
        write_u16_le(buffer, 2);  // block align
        write_u16_le(buffer, 16); // bits per sample

        write_tag(buffer, "data");
        write_u32_le(buffer, data_bytes);
        for (int16_t sample : samples)
        {
            write_u16_le(buffer, static_cast<uint16_t>(sample));
        }

        return buffer;
    }

    std::string write_temp_wav(const std::vector<uint8_t>& bytes, const std::string& name)
    {
        const std::string path = std::string(::testing::TempDir()) + name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        out.close();
        return path;
    }

    void write_fmt_and_riff_header(std::vector<uint8_t>& buffer, uint16_t format_tag,
                                   uint16_t channel_count, uint32_t sample_rate,
                                   uint16_t bits_per_sample, uint32_t data_bytes)
    {
        const uint16_t block_align = static_cast<uint16_t>(channel_count * (bits_per_sample / 8));
        const uint32_t byte_rate = sample_rate * block_align;

        write_tag(buffer, "RIFF");
        write_u32_le(buffer, 36 + data_bytes);
        write_tag(buffer, "WAVE");

        write_tag(buffer, "fmt ");
        write_u32_le(buffer, 16);
        write_u16_le(buffer, format_tag);
        write_u16_le(buffer, channel_count);
        write_u32_le(buffer, sample_rate);
        write_u32_le(buffer, byte_rate);
        write_u16_le(buffer, block_align);
        write_u16_le(buffer, bits_per_sample);

        write_tag(buffer, "data");
        write_u32_le(buffer, data_bytes);
    }

    /// @brief Builds a minimal mono 24-bit PCM WAV file.
    /// @param samples Signed values whose low 24 bits are written little-endian.
    std::vector<uint8_t> build_pcm24_wav(uint32_t sample_rate, const std::vector<int32_t>& samples)
    {
        const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 3);
        std::vector<uint8_t> buffer;
        write_fmt_and_riff_header(buffer, 1, 1, sample_rate, 24, data_bytes);
        for (int32_t sample : samples)
        {
            const uint32_t u = static_cast<uint32_t>(sample);
            buffer.push_back(static_cast<uint8_t>(u & 0xFF));
            buffer.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
        }
        return buffer;
    }

    // Builds a minimal mono 32-bit PCM WAV file.
    std::vector<uint8_t> build_pcm32_wav(uint32_t sample_rate, const std::vector<int32_t>& samples)
    {
        const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * sizeof(int32_t));
        std::vector<uint8_t> buffer;
        write_fmt_and_riff_header(buffer, 1, 1, sample_rate, 32, data_bytes);
        for (int32_t sample : samples)
        {
            write_u32_le(buffer, static_cast<uint32_t>(sample));
        }
        return buffer;
    }

    // Builds a minimal mono 32-bit IEEE float WAV file.
    std::vector<uint8_t> build_float32_wav(uint32_t sample_rate, const std::vector<float>& samples)
    {
        const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * sizeof(float));
        std::vector<uint8_t> buffer;
        write_fmt_and_riff_header(buffer, 3, 1, sample_rate, 32, data_bytes);
        for (float sample : samples)
        {
            uint32_t bits;
            std::memcpy(&bits, &sample, sizeof(float));
            write_u32_le(buffer, bits);
        }
        return buffer;
    }

    /// @brief Builds a minimal stereo 16-bit PCM WAV file, interleaved left then right.
    /// @pre left and right have the same length.
    std::vector<uint8_t> build_pcm16_stereo_wav(uint32_t sample_rate,
                                                const std::vector<int16_t>& left,
                                                const std::vector<int16_t>& right)
    {
        const uint32_t data_bytes =
            static_cast<uint32_t>((left.size() + right.size()) * sizeof(int16_t));
        std::vector<uint8_t> buffer;
        write_fmt_and_riff_header(buffer, 1, 2, sample_rate, 16, data_bytes);
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            write_u16_le(buffer, static_cast<uint16_t>(left[i]));
            write_u16_le(buffer, static_cast<uint16_t>(right[i]));
        }
        return buffer;
    }
} // namespace

TEST(Medium_WavFile, WavFileLoadsMonoPcm16SamplesInCorrectRange)
{
    const std::vector<int16_t> samples = {0, 16384, -16384, 32767, -32768};
    const std::string path = write_temp_wav(build_pcm16_wav(48000, samples), "wav_pcm16.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), samples.size());
    EXPECT_NEAR(loaded[0], 0.0f, 1e-4f);
    EXPECT_NEAR(loaded[1], 0.5f, 1e-3f);
    EXPECT_NEAR(loaded[2], -0.5f, 1e-3f);
    EXPECT_NEAR(loaded[3], 1.0f, 1e-3f);
    EXPECT_NEAR(loaded[4], -1.0f, 1e-3f);
}

TEST(Medium_WavFile, WavFileReportsTheFileSampleRate)
{
    const std::vector<int16_t> samples = {0, 0};
    const std::string path = write_temp_wav(build_pcm16_wav(44100, samples), "wav_rate.wav");

    double sample_rate = 0.0;
    SushiDSP::WavFile::load_mono(path, &sample_rate);

    EXPECT_EQ(sample_rate, 44100.0);
}

TEST(Medium_WavFile, WavFileThrowsOnMissingFile)
{
    EXPECT_THROW(SushiDSP::WavFile::load_mono("this_file_does_not_exist_12345.wav"),
                 std::runtime_error);
}

TEST(Medium_WavFile, WavFileThrowsOnTruncatedHeader)
{
    std::vector<uint8_t> truncated = {'R', 'I', 'F', 'F', 0, 0};
    const std::string path = write_temp_wav(truncated, "wav_truncated.wav");

    EXPECT_THROW(SushiDSP::WavFile::load_mono(path), std::runtime_error);
}

TEST(Medium_WavFile, WavFileLoadsMonoPcm24SamplesWithSignExtension)
{
    // The samples cover zero, both extremes and -1 LSB. See tests/foundation/io/README.md.
    const std::vector<int32_t> samples = {0x000000, 0x400000, 0x7FFFFF,
                                          static_cast<int32_t>(0xFFFFFFFF),
                                          static_cast<int32_t>(0xFF800000)};
    const std::string path = write_temp_wav(build_pcm24_wav(48000, samples), "wav_pcm24.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), samples.size());
    EXPECT_NEAR(loaded[0], 0.0f, 1e-6f);
    EXPECT_NEAR(loaded[1], 0.5f, 1e-6f);
    EXPECT_NEAR(loaded[2], 1.0f, 1e-4f);
    EXPECT_NEAR(loaded[3], -1.0f / 8388608.0f, 1e-6f);
    EXPECT_NEAR(loaded[4], -1.0f, 1e-6f);
}

TEST(Medium_WavFile, WavFileLoadsMonoPcm32SamplesInCorrectRange)
{
    const std::vector<int32_t> samples = {0, 1073741824, -1073741824, 2147483647, -2147483647 - 1};
    const std::string path = write_temp_wav(build_pcm32_wav(48000, samples), "wav_pcm32.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), samples.size());
    EXPECT_NEAR(loaded[0], 0.0f, 1e-6f);
    EXPECT_NEAR(loaded[1], 0.5f, 1e-6f);
    EXPECT_NEAR(loaded[2], -0.5f, 1e-6f);
    EXPECT_NEAR(loaded[3], 1.0f, 1e-6f);
    EXPECT_NEAR(loaded[4], -1.0f, 1e-6f);
}

TEST(Medium_WavFile, WavFileLoadsMonoIeeeFloatSamplesUnscaled)
{
    const std::vector<float> samples = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f};
    const std::string path = write_temp_wav(build_float32_wav(48000, samples), "wav_float32.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        // IEEE float samples pass through via memcpy, so they should be exact.
        EXPECT_FLOAT_EQ(loaded[i], samples[i]);
    }
}

TEST(Medium_WavFile, WavFileDownmixesStereoAsAverageOfChannels)
{
    // Same-sign values make a sum twice the average. See tests/foundation/io/README.md.
    const std::vector<int16_t> left = {0, 16384, 8000, 20000};
    const std::vector<int16_t> right = {0, 8192, 12000, 10000};
    const std::string path =
        write_temp_wav(build_pcm16_stereo_wav(48000, left, right), "wav_pcm16_stereo.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), left.size());
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        const float expected =
            ((static_cast<float>(left[i]) / 32768.0f) + (static_cast<float>(right[i]) / 32768.0f)) /
            2.0f;
        EXPECT_NEAR(loaded[i], expected, 1e-4f);
    }
    // Confirms it's a genuine average and not just one channel passed through.
    EXPECT_NE(loaded[1], static_cast<float>(left[1]) / 32768.0f);
    EXPECT_NE(loaded[1], static_cast<float>(right[1]) / 32768.0f);
}
