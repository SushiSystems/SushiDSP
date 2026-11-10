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
    using SushiDSP::host::AudioImportCache;
    using SushiDSP::host::InMemorySourceStatReader;
    using SushiDSP::host::SourceStat;

    constexpr double K_RATE_48K = 48000.0;
    constexpr double K_RATE_44K = 44100.0;

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

    /// @brief Builds a reader that reports one size and write time for a path.
    /// @param path The source path.
    /// @return A reader holding that one entry.
    InMemorySourceStatReader reader_with(const std::string& path)
    {
        InMemorySourceStatReader reader;
        reader.set(path, SourceStat{3ull, 7ll});
        return reader;
    }

    /// @brief Holds one extension and whether the cache imports it.
    struct ExtensionRow
    {
        const char* path;
        bool imported;
    };

    class Small_AudioImportCacheExtensions : public ::testing::TestWithParam<ExtensionRow>
    {
    };
} // namespace

TEST_P(Small_AudioImportCacheExtensions, ImportsOnlyMp3)
{
    EXPECT_EQ(AudioImportCache::can_import(GetParam().path), GetParam().imported);
}

INSTANTIATE_TEST_SUITE_P(Extensions, Small_AudioImportCacheExtensions,
                         ::testing::Values(ExtensionRow{"C:/songs/track.mp3", true},
                                           ExtensionRow{"C:/songs/track.MP3", true},
                                           ExtensionRow{"C:/songs/take.wav", false},
                                           ExtensionRow{"C:/songs/take", false},
                                           ExtensionRow{"C:/songs/take.flac", false}));

TEST(Small_AudioImportCache, AudioImportCacheNamesOneEntryPerSourceAndRate)
{
    const std::string source = "C:/songs/song.mp3";
    const InMemorySourceStatReader reader = reader_with(source);

    const std::string at_48 = AudioImportCache::entry_name(source, K_RATE_48K, reader);
    const std::string at_44 = AudioImportCache::entry_name(source, K_RATE_44K, reader);

    EXPECT_EQ(at_48, AudioImportCache::entry_name(source, K_RATE_48K, reader));
    EXPECT_NE(at_48, at_44);
    EXPECT_EQ(at_48.rfind("song_", 0), 0u);
    EXPECT_NE(at_48.find("_48000.wav"), std::string::npos);
    EXPECT_NE(at_44.find("_44100.wav"), std::string::npos);
}

TEST(Medium_AudioImportCache, AudioImportCacheNamesANewEntryWhenTheSourceChanges)
{
    const std::filesystem::path root = make_fixture("changed");
    const std::string source = (root / "song.mp3").string();

    write_file(source, "one");
    const std::string before = AudioImportCache::entry_name(source, K_RATE_48K);
    write_file(source, "one and a half");
    const std::string after = AudioImportCache::entry_name(source, K_RATE_48K);

    EXPECT_NE(before, after);
}

TEST(Small_AudioImportCache, AudioImportCacheKeepsOnlySafeCharactersFromTheStem)
{
    const std::string source = "C:/songs/Peace Sells! (320 Kbps).mp3";
    const InMemorySourceStatReader reader = reader_with(source);

    const std::string name = AudioImportCache::entry_name(source, K_RATE_48K, reader);

    EXPECT_EQ(name.rfind("Peace_Sells___320_Kbps__", 0), 0u);
}

TEST(Small_AudioImportCache, AudioImportCachePutsItsEntriesInItsOwnFolder)
{
    const std::string source = "C:/songs/song.mp3";
    const InMemorySourceStatReader reader = reader_with(source);

    const AudioImportCache cache("cache_folder", &reader);
    const std::filesystem::path entry(cache.entry_path(source, K_RATE_48K));

    EXPECT_EQ(entry.parent_path(), std::filesystem::path("cache_folder"));
    EXPECT_EQ(entry.filename().string(), AudioImportCache::entry_name(source, K_RATE_48K, reader));
}

TEST(Medium_AudioImportCache, AudioImportCacheReusesTheWavItAlreadyHolds)
{
    const std::filesystem::path root = make_fixture("reuse");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "not an mp3 at all");

    const AudioImportCache cache((root / "cache").string());
    const std::string entry = cache.entry_path(source, K_RATE_48K);
    std::filesystem::create_directories(root / "cache");
    write_file(entry, "kept");

    // An untouched file proves that no decode ran. See tests/applications/host_gui/README.md.
    EXPECT_EQ(cache.import(source, K_RATE_48K), entry);
    EXPECT_EQ(read_file(entry), "kept");
}

TEST(Medium_AudioImportCache, AudioImportCacheRefusesAFileThatIsNotAnMp3AndLeavesNothingBehind)
{
    const std::filesystem::path root = make_fixture("refuse");
    const std::string source = (root / "song.mp3").string();
    write_file(source, "not an mp3 at all");

    const AudioImportCache cache((root / "cache").string());

    EXPECT_THROW(cache.import(source, K_RATE_48K), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(cache.entry_path(source, K_RATE_48K)));
    EXPECT_FALSE(std::filesystem::exists(cache.entry_path(source, K_RATE_48K) + ".part"));
}
