/**************************************************************************/
/* test_behaviour.cpp                                                     */
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
 * @file test_behaviour.cpp
 * @brief Tests the directory listing's ordering and extension filter.
 * @author Mustafa Garip
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ui/directory_listing.hpp"

namespace
{
    /// @brief The directory name every in-memory case lists.
    const std::string K_DIRECTORY = "browsed";

    /// @brief Builds a source holding one directory.
    /// @param entries The directory's entries.
    /// @return The source.
    ui::InMemoryDirectorySource source_of(std::vector<ui::DirectoryEntry> entries)
    {
        ui::InMemoryDirectorySource source;
        source.add_directory(K_DIRECTORY, std::move(entries));
        return source;
    }

    /// @brief Lists the in-memory directory for a set of extensions.
    /// @param source The source that holds the directory.
    /// @param extensions The extensions to keep.
    /// @return The listing.
    std::vector<ui::DirectoryEntry> list(const ui::InMemoryDirectorySource& source,
                                         const std::vector<std::string>& extensions)
    {
        return ui::list_directory(K_DIRECTORY, extensions, source);
    }

    /// @brief Reads back the names a listing carries, in listing order.
    /// @param entries The listing.
    /// @return The names.
    std::vector<std::string> names_of(const std::vector<ui::DirectoryEntry>& entries)
    {
        std::vector<std::string> names;
        names.reserve(entries.size());
        for (const ui::DirectoryEntry& entry : entries)
        {
            names.push_back(entry.name);
        }
        return names;
    }

    /// @brief Reads back which entries of a listing are directories, in listing order.
    /// @param entries The listing.
    /// @return The directory flags.
    std::vector<bool> directory_flags_of(const std::vector<ui::DirectoryEntry>& entries)
    {
        std::vector<bool> flags;
        flags.reserve(entries.size());
        for (const ui::DirectoryEntry& entry : entries)
        {
            flags.push_back(entry.is_directory);
        }
        return flags;
    }

    /// @brief Creates an empty fixture directory under the system temp directory, replacing one
    /// an interrupted run left behind.
    /// @param leaf The directory's own name, unique within this file.
    /// @return The directory's path.
    std::filesystem::path make_fixture(const std::string& leaf)
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("sushidsp_listing_" + leaf);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
    }
} // namespace

TEST(Small_DirectoryListing, ListDirectoryPutsSubdirectoriesBeforeMatchingFiles)
{
    const ui::InMemoryDirectorySource source =
        source_of({{"b.wav", false}, {"sub", true}, {"a.wav", false}, {"notes.txt", false}});

    const std::vector<ui::DirectoryEntry> entries = list(source, {".wav"});

    EXPECT_EQ(directory_flags_of(entries), (std::vector<bool>{true, false, false}));
}

TEST(Small_DirectoryListing, ListDirectorySortsEachGroupByNameIgnoringCase)
{
    const ui::InMemoryDirectorySource source = source_of(
        {{"beta.wav", false}, {"Zulu", true}, {"alpha.wav", false}, {"Yankee", true}, {"Charlie.wav", false}});

    const std::vector<ui::DirectoryEntry> entries = list(source, {".wav"});

    EXPECT_EQ(names_of(entries),
              (std::vector<std::string>{"Yankee", "Zulu", "alpha.wav", "beta.wav", "Charlie.wav"}));
}

TEST(Small_DirectoryListing, ListDirectoryMatchesTheExtensionIgnoringCase)
{
    const ui::InMemoryDirectorySource source = source_of({{"Cab.WAV", false}});

    EXPECT_EQ(names_of(list(source, {".wav"})), (std::vector<std::string>{"Cab.WAV"}));
}

TEST(Small_DirectoryListing, ListDirectoryWithAnEmptyExtensionKeepsEveryFile)
{
    const ui::InMemoryDirectorySource source =
        source_of({{"a.wav", false}, {"notes.txt", false}, {"readme", false}});

    EXPECT_EQ(names_of(list(source, {""})),
              (std::vector<std::string>{"a.wav", "notes.txt", "readme"}));
}

TEST(Small_DirectoryListing, ListDirectoryKeepsEveryExtensionItIsGiven)
{
    const ui::InMemoryDirectorySource source =
        source_of({{"take.wav", false}, {"song.MP3", false}, {"sample.flac", false}});

    EXPECT_EQ(names_of(list(source, {".wav", ".mp3"})),
              (std::vector<std::string>{"song.MP3", "take.wav"}));
}

TEST(Small_DirectoryListing, ListDirectoryReturnsNothingForADirectoryTheSourceDoesNotHold)
{
    const ui::InMemoryDirectorySource source;

    EXPECT_TRUE(list(source, {".wav"}).empty());
}

TEST(Medium_DirectoryListing, ListDirectoryReturnsNothingForADirectoryThatDoesNotExist)
{
    const std::filesystem::path root = make_fixture("missing");
    std::filesystem::remove_all(root);

    EXPECT_TRUE(ui::list_directory(root.string(), ".wav").empty());
}

TEST(Medium_DirectoryListing, ListDirectoryReturnsNothingForAPathThatNamesAFile)
{
    const std::filesystem::path root = make_fixture("not_a_directory");
    const std::filesystem::path file = root / "a.wav";
    {
        std::ofstream out(file, std::ios::binary);
        out.put('x');
    }

    EXPECT_TRUE(ui::list_directory(file.string(), ".wav").empty());

    std::filesystem::remove_all(root);
}
