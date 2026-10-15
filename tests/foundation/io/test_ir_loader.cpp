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
 * @file test_ir_loader.cpp
 * @brief Tests the trimming, normalisation and resampling of IrLoader.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/ir_loader.hpp>

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

TEST(Medium_IrLoader, IrLoaderTrimsAndFadesToMaxLength)
{
    std::vector<float> samples(48000, 1.0f);
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0,
                                                SushiDSP::IrLoadOptions{200.0f, 5.0f, false});

    EXPECT_EQ(ir.samples.size(), 9600u);
    EXPECT_NEAR(ir.samples[9599], 0.0f, 1e-3f);
    EXPECT_NEAR(ir.samples[9600 - 240 - 1], 1.0f, 1e-3f);
}

TEST(Medium_IrLoader, IrLoaderNormalisesBandAverageToZeroDb)
{
    std::vector<float> samples(4096, 0.0f);
    samples[0] = 0.1f;

    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0);

    EXPECT_NEAR(ir.samples[0], 1.0f, 1e-3f);
    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
}

TEST(Medium_IrLoader, IrLoaderNormalisesShapedBandAverageToZeroDb)
{
    // A two-tap comb makes the band selection matter. See tests/foundation/io/README.md.
    std::vector<float> samples(4096, 0.0f);
    samples[0] = 1.0f;
    samples[64] = 0.9f;

    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0);

    // IrLoader pads to the next power of two at or above twice the length.
    const double measured = band_mean_decibels(ir.samples, 48000.0, 8192, 100.0, 5000.0);
    EXPECT_NEAR(measured, 0.0, 0.1);

    // The conditioning is a single scalar gain: the tap ratio must be untouched.
    EXPECT_NEAR(ir.samples[64] / ir.samples[0], 0.9f, 1e-5f);
}

TEST(Medium_IrLoader, IrLoaderLoadResamplesAWavFileToTheTargetRate)
{
    std::vector<std::int16_t> source(441);
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        source[i] = static_cast<std::int16_t>(
            16000.0 * std::sin(2.0 * PI * 500.0 * static_cast<double>(i) / 44100.0));
    }
    const std::string path = write_pcm16_wav("sushidsp_ir_loader_44k.wav", 44100, source);

    const auto ir = SushiDSP::IrLoader::load(path, 48000.0);

    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
    const auto expected_length = static_cast<double>(source.size()) * 48000.0 / 44100.0;
    EXPECT_NEAR(static_cast<double>(ir.samples.size()), expected_length, 2.0);
}
