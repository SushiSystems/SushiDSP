/**************************************************************************/
/* test_file_golden_store.cpp                                             */
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
 * @file test_file_golden_store.cpp
 * @brief Tests that the file GoldenStore writes raw float files and reads them back.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/golden_compare/file_golden_store.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /// @brief Holds the samples the round trip writes; they are exact in float.
    const std::vector<float> K_SAMPLES{0.0f, 0.5f, -0.25f, 1.0f};

    /// @brief Removes a folder it made under the temp directory when it goes out of scope.
    class TempFolder
    {
        public:
            TempFolder() : path_(std::filesystem::temp_directory_path() / "sushidsp_file_golden_store")
            {
                std::filesystem::create_directories(path_);
            }

            ~TempFolder()
            {
                std::filesystem::remove_all(path_);
            }

            TempFolder(const TempFolder&) = delete;
            TempFolder& operator=(const TempFolder&) = delete;

            /// @brief Returns the path of a file in the folder.
            std::string file(const char* name) const
            {
                return (path_ / name).string();
            }

        private:
            std::filesystem::path path_;
    };
} // namespace

TEST(Medium_FileGoldenStore, SamplesWrittenToAPathAreReadBackExactly)
{
    const TempFolder folder;
    SushiDSP::golden::FileGoldenStore store;
    ASSERT_TRUE(store.write(folder.file("entry.f32"), K_SAMPLES));

    std::vector<float> read;
    ASSERT_TRUE(store.read(folder.file("entry.f32"), read, K_SAMPLES.size()));

    EXPECT_EQ(read, K_SAMPLES);
}

TEST(Medium_FileGoldenStore, ReadingMoreThanTheFileHoldsReturnsWhatItHolds)
{
    const TempFolder folder;
    SushiDSP::golden::FileGoldenStore store;
    ASSERT_TRUE(store.write(folder.file("entry.f32"), K_SAMPLES));

    std::vector<float> read;
    ASSERT_TRUE(store.read(folder.file("entry.f32"), read, K_SAMPLES.size() + 3));

    EXPECT_EQ(read, K_SAMPLES);
}

TEST(Medium_FileGoldenStore, ReadingAMissingFileFails)
{
    const TempFolder folder;
    const SushiDSP::golden::FileGoldenStore store;
    std::vector<float> read;

    EXPECT_FALSE(store.read(folder.file("absent.f32"), read, 4));
}

TEST(Medium_FileGoldenStore, WritingToAMissingFolderFails)
{
    const TempFolder folder;
    SushiDSP::golden::FileGoldenStore store;

    EXPECT_FALSE(store.write(folder.file("no_such_folder/entry.f32"), K_SAMPLES));
}
