#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/io/WavFile.hpp"

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
        write_u16_le(buffer, 1);       // PCM
        write_u16_le(buffer, 1);       // mono
        write_u32_le(buffer, sample_rate);
        write_u32_le(buffer, byte_rate);
        write_u16_le(buffer, 2);       // block align
        write_u16_le(buffer, 16);      // bits per sample

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
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        return path;
    }
}

TEST(Unit_Dsp, WavFileLoadsMonoPcm16SamplesInCorrectRange)
{
    const std::vector<int16_t> samples = { 0, 16384, -16384, 32767, -32768 };
    const std::string path = write_temp_wav(build_pcm16_wav(48000, samples), "wav_pcm16.wav");

    const std::vector<float> loaded = SushiDSP::WavFile::load_mono(path);

    ASSERT_EQ(loaded.size(), samples.size());
    EXPECT_NEAR(loaded[0], 0.0f, 1e-4f);
    EXPECT_NEAR(loaded[1], 0.5f, 1e-3f);
    EXPECT_NEAR(loaded[2], -0.5f, 1e-3f);
    EXPECT_NEAR(loaded[3], 1.0f, 1e-3f);
    EXPECT_NEAR(loaded[4], -1.0f, 1e-3f);
}

TEST(Unit_Dsp, WavFileReportsTheFileSampleRate)
{
    const std::vector<int16_t> samples = { 0, 0 };
    const std::string path = write_temp_wav(build_pcm16_wav(44100, samples), "wav_rate.wav");

    double sample_rate = 0.0;
    SushiDSP::WavFile::load_mono(path, &sample_rate);

    EXPECT_EQ(sample_rate, 44100.0);
}

TEST(Unit_Dsp, WavFileThrowsOnMissingFile)
{
    EXPECT_THROW(SushiDSP::WavFile::load_mono("this_file_does_not_exist_12345.wav"), std::runtime_error);
}

TEST(Unit_Dsp, WavFileThrowsOnTruncatedHeader)
{
    std::vector<uint8_t> truncated = { 'R', 'I', 'F', 'F', 0, 0 };
    const std::string path = write_temp_wav(truncated, "wav_truncated.wav");

    EXPECT_THROW(SushiDSP::WavFile::load_mono(path), std::runtime_error);
}
