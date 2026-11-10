/**************************************************************************/
/* test_hrtf_files.cpp                                                    */
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
 * @file test_hrtf_files.cpp
 * @brief Tests the contract of the HrtfFiles port against its in-memory and its disk adapter.
 * @author Mustafa Garip
 */

#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "hrtf_files.hpp"
#include "support/remove_at_exit.hpp"
#include "support/wav_fixture.hpp"

namespace
{
    using SushiDSP::host::HrtfFiles;

    /// @brief Holds bytes a file keeps as they are: every value from 0 to 255 once.
    std::vector<unsigned char> every_byte()
    {
        std::vector<unsigned char> bytes;
        for (unsigned int value = 0; value < 256u; ++value)
        {
            bytes.push_back(static_cast<unsigned char>(value));
        }
        return bytes;
    }

    void expect_an_asset_reads_back_byte_for_byte(const HrtfFiles& files, const std::string& path)
    {
        std::vector<unsigned char> read;
        std::string error;

        ASSERT_TRUE(files.read(path, read, error)) << error;

        EXPECT_EQ(read, every_byte());
    }

    void expect_reading_a_missing_asset_fails_and_names_it(const HrtfFiles& files,
                                                           const std::string& path)
    {
        std::vector<unsigned char> bytes;
        std::string error;

        EXPECT_FALSE(files.read(path, bytes, error));

        EXPECT_NE(error.find(path), std::string::npos) << error;
    }
} // namespace

TEST(Small_HrtfFilesContract, InMemoryAnAssetReadsBackByteForByte)
{
    SushiDSP::host::InMemoryHrtfFiles files;
    files.add("memory/set.hrtf", every_byte());
    expect_an_asset_reads_back_byte_for_byte(files, "memory/set.hrtf");
}

TEST(Medium_HrtfFilesContract, DiskAnAssetReadsBackByteForByte)
{
    const SushiDSP::host::DiskHrtfFiles files;
    const std::string path = TestSupport::temp_fixture_path("contract_set.hrtf");
    const TestSupport::RemoveAtExit cleanup(path);
    const std::vector<unsigned char> bytes = every_byte();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    expect_an_asset_reads_back_byte_for_byte(files, path);
}

TEST(Small_HrtfFilesContract, InMemoryReadingAMissingAssetFailsAndNamesIt)
{
    const SushiDSP::host::InMemoryHrtfFiles files;
    expect_reading_a_missing_asset_fails_and_names_it(files, "memory/missing.hrtf");
}

TEST(Medium_HrtfFilesContract, DiskReadingAMissingAssetFailsAndNamesIt)
{
    const SushiDSP::host::DiskHrtfFiles files;
    expect_reading_a_missing_asset_fails_and_names_it(
        files, TestSupport::temp_fixture_path("contract_missing.hrtf"));
}
