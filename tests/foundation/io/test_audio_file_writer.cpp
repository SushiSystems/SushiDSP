/**************************************************************************/
/* test_audio_file_writer.cpp                                             */
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
 * @file test_audio_file_writer.cpp
 * @brief Tests that the production opener returns a writer that works through the port.
 * @author Mustafa Garip
 */

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/io/audio_file_writer.hpp>
#include <SushiDSP/foundation/io/wav_file.hpp>
#include <SushiDSP/foundation/io/wav_writer.hpp>

TEST(Medium_AudioFileWriter, TheWavOpenerWritesAFileThatWavFileReadsBack)
{
    const std::string path = std::string(::testing::TempDir()) + "sushidsp_audio_file_writer.wav";
    const std::vector<float> ramp = {0.0f, 0.25f, 0.5f, -0.5f};
    const float* channels[1] = {ramp.data()};

    {
        std::unique_ptr<SushiDSP::AudioFileWriter> writer =
            SushiDSP::open_float32_wav_writer(path, 48000.0, 1u);
        writer->append(channels, static_cast<std::uint32_t>(ramp.size()));
        writer->close();
        EXPECT_EQ(writer->frames_written(), ramp.size());
    }

    EXPECT_EQ(SushiDSP::WavFile::load_mono(path), ramp);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

TEST(Medium_AudioFileWriter, TheWavOpenerThrowsWhenThePathCannotBeOpened)
{
    const std::string missing =
        std::string(::testing::TempDir()) + "sushidsp_no_such_folder/take.wav";

    EXPECT_THROW(SushiDSP::open_float32_wav_writer(missing, 48000.0, 1u), std::runtime_error);
}
