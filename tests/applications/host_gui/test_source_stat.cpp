/**************************************************************************/
/* test_source_stat.cpp                                                   */
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
 * @file test_source_stat.cpp
 * @brief Covers the in-memory and the file adapter of the source stat port.
 * @author Mustafa Garip & Sushi Systems
 */

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "source_stat.hpp"

namespace
{
    using SushiDSP::host::FileSourceStatReader;
    using SushiDSP::host::InMemorySourceStatReader;
    using SushiDSP::host::SourceStat;
} // namespace

TEST(Small_SourceStat, InMemorySourceStatReaderReportsWhatWasSet)
{
    InMemorySourceStatReader reader;
    reader.set("a.mp3", SourceStat{12ull, 34ll});

    const SourceStat seen = reader.stat("a.mp3");

    EXPECT_EQ(seen.bytes, 12ull);
    EXPECT_EQ(seen.stamp, 34ll);
}

TEST(Small_SourceStat, InMemorySourceStatReaderReportsAnUnsetPathAsMissing)
{
    const InMemorySourceStatReader reader;

    const SourceStat seen = reader.stat("missing.mp3");

    EXPECT_EQ(seen.bytes, 0ull);
    EXPECT_EQ(seen.stamp, 0ll);
}

TEST(Medium_SourceStat, FileSourceStatReaderReportsTheSizeOfAFile)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "sushidsp_source_stat.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out << "12345";
    }

    const SourceStat seen = FileSourceStatReader().stat(path.string());
    std::filesystem::remove(path);

    EXPECT_EQ(seen.bytes, 5ull);
}

TEST(Medium_SourceStat, FileSourceStatReaderReportsAMissingFileAsZero)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "sushidsp_source_stat_missing.bin";
    std::filesystem::remove(path);

    const SourceStat seen = FileSourceStatReader().stat(path.string());

    EXPECT_EQ(seen.bytes, 0ull);
    EXPECT_EQ(seen.stamp, 0ll);
}
