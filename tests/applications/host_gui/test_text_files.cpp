/**************************************************************************/
/* test_text_files.cpp                                                    */
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
 * @file test_text_files.cpp
 * @brief Tests the contract of the TextFiles port against its in-memory and its disk adapter.
 * @author Mustafa Garip
 */

#include <string>

#include <gtest/gtest.h>

#include "text_files.hpp"
#include "support/remove_at_exit.hpp"
#include "support/wav_fixture.hpp"

namespace
{
    using SushiDSP::host::TextFiles;

    /// @brief Holds text a file keeps byte for byte: a carriage return, a line feed and a NUL.
    const std::string K_AWKWARD_TEXT = std::string("a\r\nb\n\0c", 7);

    void expect_written_text_reads_back_byte_for_byte(TextFiles& files, const std::string& path)
    {
        std::string error;
        ASSERT_TRUE(files.write(path, K_AWKWARD_TEXT, error)) << error;

        std::string read;
        ASSERT_TRUE(files.read(path, read, error)) << error;

        EXPECT_EQ(read, K_AWKWARD_TEXT);
    }

    void expect_a_write_replaces_what_the_file_held(TextFiles& files, const std::string& path)
    {
        std::string error;
        ASSERT_TRUE(files.write(path, "a long first text", error)) << error;
        ASSERT_TRUE(files.write(path, "short", error)) << error;

        std::string read;
        ASSERT_TRUE(files.read(path, read, error)) << error;

        EXPECT_EQ(read, "short");
    }

    void expect_reading_a_missing_file_fails_and_names_it(const TextFiles& files,
                                                          const std::string& path)
    {
        std::string text;
        std::string error;

        EXPECT_FALSE(files.read(path, text, error));

        EXPECT_NE(error.find(path), std::string::npos) << error;
    }
} // namespace

TEST(Small_TextFilesContract, InMemoryWrittenTextReadsBackByteForByte)
{
    SushiDSP::host::InMemoryTextFiles files;
    expect_written_text_reads_back_byte_for_byte(files, "memory/text.json");
}

TEST(Medium_TextFilesContract, DiskWrittenTextReadsBackByteForByte)
{
    SushiDSP::host::DiskTextFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_text.json");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_written_text_reads_back_byte_for_byte(files, path);
}

TEST(Small_TextFilesContract, InMemoryAWriteReplacesWhatTheFileHeld)
{
    SushiDSP::host::InMemoryTextFiles files;
    expect_a_write_replaces_what_the_file_held(files, "memory/replaced.json");
}

TEST(Medium_TextFilesContract, DiskAWriteReplacesWhatTheFileHeld)
{
    SushiDSP::host::DiskTextFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_replaced.json");
    const TestSupport::RemoveAtExit cleanup(path);
    expect_a_write_replaces_what_the_file_held(files, path);
}

TEST(Small_TextFilesContract, InMemoryReadingAMissingFileFailsAndNamesIt)
{
    const SushiDSP::host::InMemoryTextFiles files;
    expect_reading_a_missing_file_fails_and_names_it(files, "memory/missing.json");
}

TEST(Medium_TextFilesContract, DiskReadingAMissingFileFailsAndNamesIt)
{
    const SushiDSP::host::DiskTextFiles files;
    expect_reading_a_missing_file_fails_and_names_it(
        files, TestSupport::temp_fixture_path("contract_missing.json"));
}

TEST(Small_InMemoryTextFiles, AWriteToARefusedPathFailsAndLeavesNoFile)
{
    SushiDSP::host::InMemoryTextFiles files;
    files.refuse_writing("memory/locked.json");
    std::string error;

    EXPECT_FALSE(files.write("memory/locked.json", "text", error));

    EXPECT_NE(error.find("memory/locked.json"), std::string::npos) << error;
    EXPECT_FALSE(files.exists("memory/locked.json"));
}
