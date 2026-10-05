/****************************************************************/
/* test_directory_listing.cpp                                   */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ui/DirectoryListing.hpp"

namespace
{
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

    /// @brief Writes a one-byte file.
    /// @param path Where to write it.
    void write_file(const std::filesystem::path& path)
    {
        std::ofstream out(path, std::ios::binary);
        out.put('x');
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
}

TEST(Unit_Dsp, ListDirectoryPutsSubdirectoriesBeforeMatchingFilesAndSortsEachGroup)
{
    const std::filesystem::path root = make_fixture("ordering");
    std::filesystem::create_directory(root / "sub");
    write_file(root / "b.wav");
    write_file(root / "a.wav");
    write_file(root / "notes.txt");

    const std::vector<ui::DirectoryEntry> entries = ui::list_directory(root.string(), ".wav");

    EXPECT_EQ(names_of(entries), (std::vector<std::string>{ "sub", "a.wav", "b.wav" }));
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_TRUE(entries[0].is_directory);
    EXPECT_FALSE(entries[1].is_directory);
    EXPECT_FALSE(entries[2].is_directory);

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, ListDirectoryMatchesTheExtensionIgnoringCase)
{
    const std::filesystem::path root = make_fixture("case");
    write_file(root / "Cab.WAV");

    const std::vector<ui::DirectoryEntry> entries = ui::list_directory(root.string(), ".wav");

    EXPECT_EQ(names_of(entries), (std::vector<std::string>{ "Cab.WAV" }));

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, ListDirectoryWithAnEmptyExtensionKeepsEveryFile)
{
    const std::filesystem::path root = make_fixture("every_file");
    write_file(root / "a.wav");
    write_file(root / "notes.txt");
    write_file(root / "readme");

    const std::vector<ui::DirectoryEntry> entries = ui::list_directory(root.string(), "");

    EXPECT_EQ(names_of(entries),
              (std::vector<std::string>{ "a.wav", "notes.txt", "readme" }));

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, ListDirectoryKeepsEveryExtensionItIsGiven)
{
    const std::filesystem::path root = make_fixture("several_extensions");
    write_file(root / "take.wav");
    write_file(root / "song.MP3");
    write_file(root / "sample.flac");

    const std::vector<ui::DirectoryEntry> entries =
        ui::list_directory(root.string(), std::vector<std::string>{ ".wav", ".mp3" });

    EXPECT_EQ(names_of(entries), (std::vector<std::string>{ "song.MP3", "take.wav" }));

    std::filesystem::remove_all(root);
}

TEST(Unit_Dsp, ListDirectoryReturnsNothingForADirectoryThatDoesNotExist)
{
    const std::filesystem::path root = make_fixture("missing");
    std::filesystem::remove_all(root);

    EXPECT_TRUE(ui::list_directory(root.string(), ".wav").empty());
}

TEST(Unit_Dsp, ListDirectoryReturnsNothingForAPathThatNamesAFile)
{
    const std::filesystem::path root = make_fixture("not_a_directory");
    const std::filesystem::path file = root / "a.wav";
    write_file(file);

    EXPECT_TRUE(ui::list_directory(file.string(), ".wav").empty());

    std::filesystem::remove_all(root);
}
