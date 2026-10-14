/**************************************************************************/
/* test_audio_import_cache.cpp                                            */
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
 * @file test_audio_import_cache.cpp
 * @brief Covers the folder of decoded WAVs the host keeps beside its executable.
 * @author Mustafa Garip & Sushi Systems
 */

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "audio_import_cache.hpp"

namespace
{
    /// @brief Creates an empty fixture directory under the system temp directory, replacing one
    /// an interrupted run left behind.
    /// @param leaf The directory's own name, unique within this file.
    /// @return The directory's path.
    std::filesystem::path make_fixture(const std::string& leaf)
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("sushidsp_import_" + leaf);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
    }

    /// @brief Writes a text file.
    /// @param path Where to write it.
    /// @param text What to write.
    void write_file(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream out(path, std::ios::binary);
        out << text;
    }

    /// @brief Reads a text file back.
    /// @param path The file.
    /// @return Its contents.
    std::string read_file(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
} // namespace

TEST(Unit_Dsp, AudioImportCacheImportsOnlyMp3)
{
    EXPECT_TRUE(SushiDSP::host::AudioImportCache::can_import("C:/songs/track.mp3"));
    EXPECT_TRUE(SushiDSP::host::AudioImportCache::can_import("C:/songs/track.MP3"));
    EXPECT_FALSE(SushiDSP::host::AudioImportCache::can_import("C:/songs/take.wav"));
    EXPECT_FALSE(SushiDSP::host::AudioImportCache::can_import("C:/songs/take"));
    EXPECT_FALSE(SushiDSP::host::AudioImportCache::can_import("C:/songs/take.flac"));
}

TEST(Unit_Dsp, AudioImportCacheNamesOneEntryPerSourceAndRate)
{
    const std::filesystem::path root = make_fixture("names");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "one");

    const std::string at_48 = SushiDSP::host::AudioImportCache::entry_name(source, 48000.0);
    const std::string at_44 = SushiDSP::host::AudioImportCache::entry_name(source, 44100.0);

    EXPECT_EQ(at_48, SushiDSP::host::AudioImportCache::entry_name(source, 48000.0));
    EXPECT_NE(at_48, at_44);
    EXPECT_NE(at_48.find("song_"), std::string::npos);
    EXPECT_NE(at_48.find("_48000.wav"), std::string::npos);
    EXPECT_NE(at_44.find("_44100.wav"), std::string::npos);
}

TEST(Unit_Dsp, AudioImportCacheNamesANewEntryWhenTheSourceChanges)
{
    const std::filesystem::path root = make_fixture("changed");
    const std::string source = (root / "song.mp3").string();

    write_file(source, "one");
    const std::string before = SushiDSP::host::AudioImportCache::entry_name(source, 48000.0);
    write_file(source, "one and a half");
    const std::string after = SushiDSP::host::AudioImportCache::entry_name(source, 48000.0);

    EXPECT_NE(before, after);
}

TEST(Unit_Dsp, AudioImportCacheKeepsOnlySafeCharactersFromTheStem)
{
    const std::filesystem::path root = make_fixture("stem");
    const std::string source = (root / "Peace Sells! (320 Kbps).mp3").string();
    write_file(source, "one");

    const std::string name = SushiDSP::host::AudioImportCache::entry_name(source, 48000.0);

    EXPECT_EQ(name.rfind("Peace_Sells___320_Kbps__", 0), 0u);
}

TEST(Unit_Dsp, AudioImportCachePutsItsEntriesInItsOwnFolder)
{
    const std::filesystem::path root = make_fixture("folder");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "one");

    const SushiDSP::host::AudioImportCache cache((root / "cache").string());
    const std::filesystem::path entry(cache.entry_path(source, 48000.0));

    EXPECT_EQ(entry.parent_path(), root / "cache");
    EXPECT_EQ(entry.filename().string(),
              SushiDSP::host::AudioImportCache::entry_name(source, 48000.0));
}

TEST(Unit_Dsp, AudioImportCacheReusesTheWavItAlreadyHolds)
{
    const std::filesystem::path root = make_fixture("reuse");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "not an mp3 at all");

    const SushiDSP::host::AudioImportCache cache((root / "cache").string());
    const std::string entry = cache.entry_path(source, 48000.0);
    std::filesystem::create_directories(root / "cache");
    write_file(entry, "kept");

    // An untouched file proves that no decode ran. See tests/functional/unit/README.md.
    EXPECT_EQ(cache.import(source, 48000.0), entry);
    EXPECT_EQ(read_file(entry), "kept");
}

TEST(Unit_Dsp, AudioImportCacheRefusesAFileThatIsNotAnMp3AndLeavesNothingBehind)
{
    const std::filesystem::path root = make_fixture("refuse");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "not an mp3 at all");

    const SushiDSP::host::AudioImportCache cache((root / "cache").string());

    EXPECT_THROW(cache.import(source, 48000.0), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(cache.entry_path(source, 48000.0)));
    EXPECT_FALSE(std::filesystem::exists(cache.entry_path(source, 48000.0) + ".part"));
}
