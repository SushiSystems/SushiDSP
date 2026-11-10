/**************************************************************************/
/* test_ir_loader.cpp                                                     */
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
 * @file test_ir_loader.cpp
 * @brief Tests the trimming, normalisation and resampling of IrLoader.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/io/io_log_category.hpp>
#include <SushiDSP/foundation/io/ir_loader.hpp>
#include <SushiDSP/foundation/log_capture/log_capture.hpp>
#include <SushiDSP/foundation/log_capture/log_level_scope.hpp>

namespace
{
    constexpr double PI = 3.14159265358979323846;

    void write_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
        }
    }

    void write_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    void write_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    // Minimal mono 16-bit PCM WAV, the same layout test_wav_file.cpp builds.
    std::string write_pcm16_wav(const std::string& name, std::uint32_t sample_rate,
                                const std::vector<std::int16_t>& samples)
    {
        const auto data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));

        std::vector<std::uint8_t> buffer;
        write_tag(buffer, "RIFF");
        write_u32_le(buffer, 36 + data_bytes);
        write_tag(buffer, "WAVE");
        write_tag(buffer, "fmt ");
        write_u32_le(buffer, 16);
        write_u16_le(buffer, 1);
        write_u16_le(buffer, 1);
        write_u32_le(buffer, sample_rate);
        write_u32_le(buffer, sample_rate * 2);
        write_u16_le(buffer, 2);
        write_u16_le(buffer, 16);
        write_tag(buffer, "data");
        write_u32_le(buffer, data_bytes);
        for (std::int16_t sample : samples)
        {
            write_u16_le(buffer, static_cast<std::uint16_t>(sample));
        }

        const std::string path = std::string(::testing::TempDir()) + name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(buffer.data()),
                  static_cast<std::streamsize>(buffer.size()));
        out.close();
        return path;
    }

    /// @brief Returns the mean magnitude in dB over a band by a direct DFT of the samples.
    /// @see tests/foundation/io/README.md
    double band_mean_decibels(const std::vector<float>& samples, double sample_rate,
                              std::uint32_t fft_size, double low_hz, double high_hz)
    {
        const double bin_width = sample_rate / static_cast<double>(fft_size);
        const auto first_bin = static_cast<std::uint32_t>(std::ceil(low_hz / bin_width));
        const auto last_bin = static_cast<std::uint32_t>(std::floor(high_hz / bin_width));

        double sum = 0.0;
        std::uint32_t count = 0;
        for (std::uint32_t bin = first_bin; bin <= last_bin; ++bin)
        {
            double real = 0.0, imaginary = 0.0;
            for (std::size_t i = 0; i < samples.size(); ++i)
            {
                const double phase = 2.0 * PI * bin * static_cast<double>(i) / fft_size;
                real += samples[i] * std::cos(phase);
                imaginary -= samples[i] * std::sin(phase);
            }
            sum += 20.0 * std::log10(std::sqrt(real * real + imaginary * imaginary));
            ++count;
        }
        return sum / count;
    }
} // namespace

namespace
{
    /// @brief Holds the length of the fade the trim tests ask for, in samples: 5 ms at 48 kHz.
    constexpr std::size_t K_FADE_SAMPLES = 240;

    /// @brief Holds the length the trim tests cut to, in samples: 200 ms at 48 kHz.
    constexpr std::size_t K_TRIMMED_SAMPLES = 9600;

    /// @brief Holds the decibels in one neper of amplitude, 20 / ln 10.
    constexpr double K_DECIBELS_PER_NEPER = 8.685889638065035;

    /// @brief Holds log2 of the 8192-point transform the shaped comb is padded to.
    constexpr double K_FFT_STAGES = 13.0;

    /// @brief Holds the two comb taps and their spacing in samples.
    constexpr float K_FIRST_TAP = 1.0f;
    constexpr float K_SECOND_TAP = 0.9f;
    constexpr std::size_t K_TAP_SPACING = 64;

    /// @brief Removes the file it names when it goes out of scope, so no run leaves a fixture behind.
    class ScopedFile
    {
        public:
            explicit ScopedFile(std::string path) : path_(std::move(path))
            {
            }

            ~ScopedFile()
            {
                std::error_code ignored;
                std::filesystem::remove(path_, ignored);
            }

            ScopedFile(const ScopedFile&) = delete;
            ScopedFile& operator=(const ScopedFile&) = delete;

            /// @brief Returns the path.
            const std::string& path() const
            {
                return path_;
            }

        private:
            std::string path_;
    };

    /// @brief Returns the one-second-of-ones response, prepared to 200 ms with a 5 ms fade and no normalisation.
    SushiDSP::Ir trimmed_ir()
    {
        const std::vector<float> samples(48000, 1.0f);
        return SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0, SushiDSP::IrLoadOptions{200.0f, 5.0f, false});
    }

    /// @brief Returns a 500 Hz sine of @p count samples at 44.1 kHz, as 16-bit PCM values.
    std::vector<std::int16_t> sine_pcm16(std::size_t count)
    {
        std::vector<std::int16_t> samples(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            samples[i] =
                static_cast<std::int16_t>(16000.0 * std::sin(2.0 * PI * 500.0 * static_cast<double>(i) / 44100.0));
        }
        return samples;
    }

    /// @brief Returns the two-tap comb the shaped test normalises.
    std::vector<float> comb()
    {
        std::vector<float> samples(4096, 0.0f);
        samples[0] = K_FIRST_TAP;
        samples[K_TAP_SPACING] = K_SECOND_TAP;
        return samples;
    }

    /// @brief Bounds the band average of the prepared comb, in decibels.
    ///
    /// The loader's own transform runs in float: each bin of the 8192-point transform is within
    /// 13 * u * sum|x| of the true bin (13 stages, sum|x| = 1.9), and the weakest bin of the comb
    /// is 1 - 0.9 = 0.1 strong, so a bin's relative error is at most that over 0.1, which is
    /// 20 / ln 10 times as many decibels. The scaled samples round once more, by one u.
    double comb_band_bound_db()
    {
        const double u = TestSupport::unit_roundoff<float>();
        const double bin_error = K_FFT_STAGES * u * (K_FIRST_TAP + K_SECOND_TAP) / (K_FIRST_TAP - K_SECOND_TAP);
        return K_DECIBELS_PER_NEPER * (bin_error + u);
    }
} // namespace

TEST(Small_IrLoader, IrLoaderTrimsToMaxLength)
{
    EXPECT_EQ(trimmed_ir().samples.size(), K_TRIMMED_SAMPLES);
}

TEST(Small_IrLoader, IrLoaderFadesTheTailToTheCosineLaw)
{
    // The last fade sample, i = 239 of 240, has gain 0.5 * (1 + cos(pi * 239 / 240)) = sin^2(pi / 480).
    const double sine = std::sin(PI / 480.0);

    const SushiDSP::Ir ir = trimmed_ir();

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(ir.samples[K_TRIMMED_SAMPLES - 1], sine * sine, 4)));
}

TEST(Small_IrLoader, IrLoaderLeavesTheSamplesBeforeTheFadeUntouched)
{
    EXPECT_EQ(trimmed_ir().samples[K_TRIMMED_SAMPLES - K_FADE_SAMPLES - 1], 1.0f);
}

TEST(Small_IrLoader, IrLoaderNormalisesShapedBandAverageToZeroDb)
{
    // A two-tap comb makes the band selection matter. See tests/foundation/io/README.md.
    const auto ir = SushiDSP::IrLoader::prepare(comb(), 48000.0, 48000.0);

    // IrLoader pads to the next power of two at or above twice the length.
    const double measured = band_mean_decibels(ir.samples, 48000.0, 8192, 100.0, 5000.0);
    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_error(measured, 0.0, comb_band_bound_db())));
}

TEST(Small_IrLoader, IrLoaderNormalisationLeavesTheTapRatioUntouched)
{
    // One scalar gain leaves the ratio untouched; each tap rounds once, so the ratio is within 2 u.
    const auto ir = SushiDSP::IrLoader::prepare(comb(), 48000.0, 48000.0);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(
        ir.samples[K_TAP_SPACING] / ir.samples[0], K_SECOND_TAP / K_FIRST_TAP, 2)));
}

TEST(Medium_IrLoader, IrLoaderLoadResamplesAWavFileToTheTargetRate)
{
    const ScopedFile file(write_pcm16_wav("sushidsp_ir_loader_44k.wav", 44100, sine_pcm16(441)));

    const auto ir = SushiDSP::IrLoader::load(file.path(), 48000.0);

    // 441 samples at 44.1 kHz are 10 ms, exactly 480 samples at 48 kHz, so the rounded count is exact.
    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
    EXPECT_EQ(ir.samples.size(), 480u);
}

TEST(Small_IrLoaderLogging, IrLoaderLogsOneInfoNamingBothRatesWhenItResamples)
{
    const std::vector<float> samples(441, 0.1f);

    const SushiDSP::LogLevelScope level(K_IO_LOG_CATEGORY, SushiLog::LogLevel::Info);
    SushiDSP::LogCapture capture;
    const auto ir = SushiDSP::IrLoader::prepare(samples, 44100.0, 48000.0);

    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Info);
    EXPECT_EQ(lines[0].category, "io");
    EXPECT_NE(lines[0].message.find("44100"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("48000"), std::string::npos) << lines[0].message;
}

TEST(Small_IrLoaderLogging, IrLoaderLogsOneWarnNamingBothLengthsWhenItCutsTheTail)
{
    const std::vector<float> samples(9600, 0.1f);
    SushiDSP::IrLoadOptions options;
    options.max_length_ms = 100.0f;

    SushiDSP::LogCapture capture;
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0, options);

    EXPECT_EQ(ir.samples.size(), 4800u);
    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "io");
    EXPECT_NE(lines[0].message.find("200 ms"), std::string::npos) << lines[0].message;
    EXPECT_NE(lines[0].message.find("100 ms"), std::string::npos) << lines[0].message;
}

TEST(Small_IrLoaderLogging, IrLoaderWritesNothingForAMatchingRateAndAShortResponse)
{
    const std::vector<float> samples(4800, 0.1f);

    SushiDSP::LogCapture capture;
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0);

    EXPECT_EQ(ir.samples.size(), 4800u);
    EXPECT_TRUE(capture.lines().empty());
}
