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
 * @file test_wav_writer.cpp
 * @brief Tests that WavWriter writes files WavFile reads back.
 * @author Mustafa Garip
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/wav_file.hpp>
#include <SushiDSP/foundation/io/wav_writer.hpp>

#include "support/tolerance.hpp"

namespace
{
    constexpr double K_PI = 3.14159265358979323846;

    /// @brief The peak magnitude of the sines below.
    constexpr float K_SINE_AMPLITUDE = 0.5f;

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
            const std::string& path() const
            {
                return path_;
            }

        private:
            /// @brief Holds the file's full path.
            std::string path_;
    };

    std::vector<float> sine(std::size_t count, double frequency, double sample_rate)
    {
        std::vector<float> samples(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            samples[i] = static_cast<float>(static_cast<double>(K_SINE_AMPLITUDE) *
                                            std::sin(2.0 * K_PI * frequency * static_cast<double>(i) / sample_rate));
        }
        return samples;
    }

    /// @brief Returns a signal scaled by a factor.
    std::vector<float> scaled(const std::vector<float>& signal, float factor)
    {
        std::vector<float> result(signal.size());
        std::transform(signal.begin(), signal.end(), result.begin(), [factor](float sample) { return factor * sample; });
        return result;
    }

    /// @brief Writes one block of mono samples to a file in a format and closes the writer.
    void write_mono(const std::string& path, SushiDSP::WavWriter::Format format, const std::vector<float>& samples)
    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, format);
        const float* channels[1] = {samples.data()};
        writer.append(channels, static_cast<std::uint32_t>(samples.size()));
    }

    /// @brief Returns the largest absolute difference between two signals of the same length.
    double max_abs_difference(const std::vector<float>& a, const std::vector<float>& b)
    {
        double worst = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            worst = std::max(worst, std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i])));
        }
        return worst;
    }

    /// @brief Returns the error ratio of the sample that uses the most of its rounding bound, for a
    /// read-back mean of two channels against 0.5 * (left + right).
    ///
    /// The reader adds two floats and halves the sum, so the bound is two roundings of
    /// the magnitude sum |left| + |right| (Higham, Accuracy and Stability, section 3.1).
    TestSupport::ErrorRatio worst_mean_error(const std::vector<float>& readback, const std::vector<float>& left,
                                             const std::vector<float>& right)
    {
        TestSupport::ErrorRatio worst;
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const double expected = 0.5 * (static_cast<double>(left[i]) + static_cast<double>(right[i]));
            const double magnitude = std::fabs(static_cast<double>(left[i])) + std::fabs(static_cast<double>(right[i]));
            const TestSupport::ErrorRatio measured = TestSupport::measure_sum_error<float>(readback[i], expected, magnitude, 2);
            if (measured.ratio > worst.ratio)
            {
                worst = measured;
            }
        }
        return worst;
    }

    /// @brief Reads a whole file as bytes.
    std::vector<std::uint8_t> read_file_bytes(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    /// @brief Reads a little-endian 32-bit value at a byte offset.
    std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    /// @brief Reads a little-endian 16-bit value at a byte offset.
    std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
    }

    /// @brief Reads the four-character tag at a byte offset.
    std::string read_tag(const std::vector<std::uint8_t>& bytes, std::size_t offset)
    {
        return std::string(reinterpret_cast<const char*>(bytes.data() + offset), 4);
    }

    /// @brief Names a format, the blocks appended to it and where its header keeps the sizes.
    struct SizesRow
    {
            const char* name;
            SushiDSP::WavWriter::Format format;
            std::vector<std::size_t> block_frames;
            std::size_t bytes_per_sample;
            std::size_t header_bytes;
            std::uint32_t riff_overhead_bytes;
            std::size_t data_size_offset;
    };

    /// @brief Returns the row's name, for the parameterised test names.
    std::string row_name(const ::testing::TestParamInfo<SizesRow>& info)
    {
        return std::string(info.param.name);
    }

    /// @brief Appends the blocks of a row to a writer, closes it, and returns the frames it counted.
    std::uint64_t write_blocks(const std::string& path, const SizesRow& row)
    {
        SushiDSP::WavWriter writer(path, 48000.0, 1, row.format);
        for (const std::size_t frames : row.block_frames)
        {
            const std::vector<float> block = sine(frames, 220.0, 48000.0);
            const float* channels[1] = {block.data()};
            writer.append(channels, static_cast<std::uint32_t>(block.size()));
        }
        return writer.frames_written();
    }

    /// @brief Sums the frames of a row's blocks.
    std::size_t total_frames(const SizesRow& row)
    {
        std::size_t total = 0;
        for (const std::size_t frames : row.block_frames)
        {
            total += frames;
        }
        return total;
    }
} // namespace

TEST(Medium_WavWriter, WavWriterFloat32RoundTripsSampleForSample)
{
    const TempWavFile fixture("sushidsp_wav_writer_float32.wav");
    const std::vector<float> samples = sine(256, 440.0, 48000.0);
    write_mono(fixture.path(), SushiDSP::WavWriter::Format::Float32, samples);

    EXPECT_EQ(SushiDSP::WavFile::load_mono(fixture.path()), samples);
}

TEST(Medium_WavWriter, WavWriterPcm16RoundTripsWithinOneQuantisationStep)
{
    const TempWavFile fixture("sushidsp_wav_writer_pcm16.wav");
    const std::vector<float> samples = sine(256, 440.0, 48000.0);
    // The writer's 32767 scale and the reader's 32768 divide bound a sample of magnitude a at (0.5 + a) / 32768.
    const double bound = (0.5 + static_cast<double>(K_SINE_AMPLITUDE)) / 32768.0;
    write_mono(fixture.path(), SushiDSP::WavWriter::Format::Pcm16, samples);

    const auto readback = SushiDSP::WavFile::load_mono(fixture.path());

    ASSERT_EQ(readback.size(), samples.size());
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(max_abs_difference(readback, samples), 0.0, bound, 1.0)));
}

TEST(Medium_WavWriter, WavWriterTwoChannelsDownmixToTheMeanOnReadback)
{
    const TempWavFile fixture("sushidsp_wav_writer_stereo.wav");
    const std::vector<float> left = sine(128, 220.0, 48000.0);
    const std::vector<float> right = scaled(left, -0.5f);
    {
        SushiDSP::WavWriter writer(fixture.path(), 48000.0, 2, SushiDSP::WavWriter::Format::Float32);
        const float* channels[2] = {left.data(), right.data()};
        writer.append(channels, static_cast<std::uint32_t>(left.size()));
    }

    const auto readback = SushiDSP::WavFile::load_mono(fixture.path());

    ASSERT_EQ(readback.size(), left.size());
    EXPECT_TRUE(TestSupport::is_within(worst_mean_error(readback, left, right)));
}

TEST(Medium_WavWriter, WavWriterSecondCloseLeavesTheFrameCountAlone)
{
    const TempWavFile fixture("sushidsp_wav_writer_double_close_count.wav");
    const std::vector<float> samples = sine(64, 220.0, 48000.0);
    SushiDSP::WavWriter writer(fixture.path(), 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
    const float* channels[1] = {samples.data()};
    writer.append(channels, static_cast<std::uint32_t>(samples.size()));
    writer.close();

    writer.close();

    EXPECT_EQ(writer.frames_written(), samples.size());
}

TEST(Medium_WavWriter, WavWriterFileIsValidAfterCloseTwice)
{
    const TempWavFile fixture("sushidsp_wav_writer_double_close_file.wav");
    const std::vector<float> samples = sine(64, 220.0, 48000.0);
    {
        SushiDSP::WavWriter writer(fixture.path(), 48000.0, 1, SushiDSP::WavWriter::Format::Float32);
        const float* channels[1] = {samples.data()};
        writer.append(channels, static_cast<std::uint32_t>(samples.size()));
        writer.close();
        writer.close();
    }

    EXPECT_EQ(SushiDSP::WavFile::load_mono(fixture.path()), samples);
}

class Medium_WavWriterSizes : public ::testing::TestWithParam<SizesRow>
{
};

TEST_P(Medium_WavWriterSizes, WavWriterHeaderAndFileSizesFollowTheFramesAppended)
{
    const SizesRow& row = GetParam();
    const TempWavFile fixture(std::string("sushidsp_wav_writer_sizes_") + row.name + ".wav");
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(total_frames(row) * row.bytes_per_sample);

    const std::uint64_t counted = write_blocks(fixture.path(), row);
    const std::vector<std::uint8_t> bytes = read_file_bytes(fixture.path());

    EXPECT_EQ(counted, total_frames(row));
    ASSERT_GE(bytes.size(), row.data_size_offset + 4u);
    EXPECT_EQ(read_u32_le(bytes, 4), row.riff_overhead_bytes + data_bytes);
    EXPECT_EQ(read_u32_le(bytes, row.data_size_offset), data_bytes);
    EXPECT_EQ(bytes.size(), row.header_bytes + data_bytes);
}

// A 44-byte PCM header counts 36 bytes after the RIFF size; the 58-byte float header adds the 18-byte fmt chunk and the fact chunk, 50 bytes.
INSTANTIATE_TEST_SUITE_P(Rows, Medium_WavWriterSizes,
                         ::testing::Values(SizesRow{"Pcm16TwoAppends", SushiDSP::WavWriter::Format::Pcm16, {100, 37}, 2, 44, 36, 40},
                                           SizesRow{"Float32OneAppend", SushiDSP::WavWriter::Format::Float32, {50}, 4, 58, 50, 54}),
                         row_name);

TEST(Medium_WavWriter, WavWriterFloat32HeaderCarriesAnEighteenByteFloatFmtChunkAndAFactChunk)
{
    const TempWavFile fixture("sushidsp_wav_writer_fact_chunk.wav");
    const std::vector<float> samples = sine(50, 440.0, 48000.0);
    write_mono(fixture.path(), SushiDSP::WavWriter::Format::Float32, samples);

    const std::vector<std::uint8_t> bytes = read_file_bytes(fixture.path());

    ASSERT_GE(bytes.size(), 58u);
    EXPECT_EQ(read_tag(bytes, 0), "RIFF");
    EXPECT_EQ(read_tag(bytes, 8), "WAVE");
    EXPECT_EQ(read_tag(bytes, 12), "fmt ");
    EXPECT_EQ(read_u32_le(bytes, 16), 18u);
    EXPECT_EQ(read_u16_le(bytes, 20), 3u); // IEEE float format tag.
    EXPECT_EQ(read_u16_le(bytes, 36), 0u); // cbSize.
    EXPECT_EQ(read_tag(bytes, 38), "fact");
    EXPECT_EQ(read_u32_le(bytes, 42), 4u);
    EXPECT_EQ(read_u32_le(bytes, 46), samples.size());
    EXPECT_EQ(read_tag(bytes, 50), "data");
}

TEST(Medium_WavWriter, WavWriterThrowsWhenThePathCannotBeOpened)
{
    const TempWavFile fixture("sushidsp_wav_writer_no_such_dir/take.wav");
    const std::string& path = fixture.path();

    EXPECT_THROW(SushiDSP::WavWriter(path, 48000.0, 1, SushiDSP::WavWriter::Format::Float32),
                 std::runtime_error);
}
