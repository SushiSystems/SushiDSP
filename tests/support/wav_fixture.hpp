/**************************************************************************/
/* wav_fixture.hpp                                                        */
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
 * @file wav_fixture.hpp
 * @brief Writes the small WAV files the rig tests read back, each in the test temporary folder.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

namespace TestSupport
{
    /// @brief Names a fixture file after the running case, so two cases ctest schedules in
    /// parallel never write the same path.
    /// @param name The file's name inside the test temporary directory.
    /// @return The full path, with the running case's name prefixed. A parameterised case's name
    /// holds a '/' before its row, which would name a folder that does not exist, so each '/'
    /// reads as '_'.
    inline std::string temp_fixture_path(const std::string& name)
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string prefix = info != nullptr ? std::string(info->name()) + "_" : "";
        std::replace(prefix.begin(), prefix.end(), '/', '_');
        return std::string(::testing::TempDir()) + "sushidsp_" + prefix + name;
    }

    /// @brief Appends a 32-bit little-endian word.
    inline void append_u32_le(std::vector<std::uint8_t>& buffer, std::uint32_t value)
    {
        for (int byte = 0; byte < 4; ++byte)
        {
            buffer.push_back(static_cast<std::uint8_t>((value >> (8 * byte)) & 0xFF));
        }
    }

    /// @brief Appends a 16-bit little-endian word.
    inline void append_u16_le(std::vector<std::uint8_t>& buffer, std::uint16_t value)
    {
        buffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    }

    /// @brief Appends a four-character chunk tag.
    inline void append_tag(std::vector<std::uint8_t>& buffer, const char* tag)
    {
        buffer.insert(buffer.end(), tag, tag + 4);
    }

    /// @brief Writes bytes to a file in the test temporary folder.
    /// @param name The file's name inside the folder.
    /// @param bytes The file's content.
    /// @return The file's full path.
    inline std::string write_fixture_bytes(const std::string& name,
                                           const std::vector<std::uint8_t>& bytes)
    {
        const std::string path = temp_fixture_path(name);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    /// @brief Writes a 16-bit mono WAV file of 64 samples that is silent but for one impulse.
    /// @return The file's full path.
    inline std::string write_temp_impulse_wav()
    {
        const std::uint32_t sample_rate = 48000;
        std::vector<std::int16_t> samples(64, 0);
        samples[0] = 16384;

        const std::uint32_t data_bytes =
            static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 2);
        append_u16_le(bytes, 2);
        append_u16_le(bytes, 16);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (std::int16_t sample : samples)
        {
            append_u16_le(bytes, static_cast<std::uint16_t>(sample));
        }
        return write_fixture_bytes("cabinet.wav", bytes);
    }

    /// @brief Writes a 32-bit-float mono WAV file, whose round trip through WavFile is exact.
    /// @param name The file's name inside the test temporary directory.
    /// @param samples The audio to write.
    /// @param sample_rate The rate to record in the header.
    /// @return The file's full path.
    inline std::string write_temp_float_wav(const char* name, const std::vector<float>& samples,
                                            std::uint32_t sample_rate)
    {
        const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
        std::vector<std::uint8_t> bytes;
        append_tag(bytes, "RIFF");
        append_u32_le(bytes, 36 + data_bytes);
        append_tag(bytes, "WAVE");
        append_tag(bytes, "fmt ");
        append_u32_le(bytes, 16);
        append_u16_le(bytes, 3);
        append_u16_le(bytes, 1);
        append_u32_le(bytes, sample_rate);
        append_u32_le(bytes, sample_rate * 4);
        append_u16_le(bytes, 4);
        append_u16_le(bytes, 32);
        append_tag(bytes, "data");
        append_u32_le(bytes, data_bytes);
        for (float sample : samples)
        {
            std::uint32_t word = 0;
            std::memcpy(&word, &sample, sizeof(word));
            append_u32_le(bytes, word);
        }
        return write_fixture_bytes(name, bytes);
    }

    /// @brief Builds a repeatable guitar-shaped test signal: a 0.4 sine at 220 Hz sampled at 48 kHz.
    /// @param frames How many samples to produce.
    /// @return The signal.
    inline std::vector<float> make_test_signal(std::size_t frames)
    {
        std::vector<float> signal(frames, 0.0f);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const double phase =
                2.0 * 3.14159265358979323846 * 220.0 * static_cast<double>(frame) / 48000.0;
            signal[frame] = 0.4f * static_cast<float>(std::sin(phase));
        }
        return signal;
    }

    /// @brief Fails unless two signals agree on every sample, bit for bit.
    inline void expect_same_signal(const std::vector<float>& actual,
                                   const std::vector<float>& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t frame = 0; frame < actual.size(); ++frame)
        {
            EXPECT_TRUE(is_within(measure_error(actual[frame], expected[frame], 0.0)))
                << "frame " << frame;
        }
    }

    /// @brief Strips a path down to its file name without an extension, the way the controller
    /// names a lane after the file dropped onto it.
    /// @param path The path to reduce.
    /// @return The stem.
    inline std::string path_stem_of(const std::string& path)
    {
        const std::size_t cut = path.find_last_of("/\\");
        const std::string name = cut == std::string::npos ? path : path.substr(cut + 1);
        const std::size_t dot = name.find_last_of('.');
        return dot == std::string::npos ? name : name.substr(0, dot);
    }
} // namespace TestSupport
