/**************************************************************************/
/* test_startup_rig.cpp                                                   */
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
 * @file test_startup_rig.cpp
 * @brief Tests the reader of the startup rig, and the record it writes for a file that will not open.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include "startup_rig.hpp"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using SushiDSP::Plugin::FileRigFileReader;
    using SushiDSP::Plugin::InMemoryRigFileReader;
    using SushiDSP::Plugin::read_startup_rig;

    /// @brief The path the in-memory cases name.
    const std::string K_PATH = "rigs/startup.json";

    /// @brief The text of the preset the cases read.
    const std::string K_TEXT = "{\"rig\": 1}";

    /// @brief Sets the startup variable for the lifetime of the object.
    class EnvironmentValue final
    {
        public:
            /// @brief Sets the variable to @p value; an empty value clears it.
            explicit EnvironmentValue(const std::string& value)
            {
#if defined(_WIN32)
                _putenv_s(SushiDSP::Plugin::K_STARTUP_RIG_VARIABLE, value.c_str());
#else
                if (value.empty())
                    unsetenv(SushiDSP::Plugin::K_STARTUP_RIG_VARIABLE);
                else
                    setenv(SushiDSP::Plugin::K_STARTUP_RIG_VARIABLE, value.c_str(), 1);
#endif
            }

            EnvironmentValue(const EnvironmentValue&) = delete;
            EnvironmentValue& operator=(const EnvironmentValue&) = delete;

            /// @brief Clears the variable.
            ~EnvironmentValue()
            {
#if defined(_WIN32)
                _putenv_s(SushiDSP::Plugin::K_STARTUP_RIG_VARIABLE, "");
#else
                unsetenv(SushiDSP::Plugin::K_STARTUP_RIG_VARIABLE);
#endif
            }
    };

    /// @brief A folder under the system temp folder that is removed when the object ends, whatever
    /// the test does.
    class TemporaryFolder final
    {
        public:
            /// @brief Creates the folder, replacing one an interrupted run left behind.
            TemporaryFolder()
                : path_(std::filesystem::temp_directory_path() /
                        ("sushidsp_startup_rig_" + std::to_string(next_index_++)))
            {
                std::filesystem::remove_all(path_);
                std::filesystem::create_directories(path_);
            }

            TemporaryFolder(const TemporaryFolder&) = delete;
            TemporaryFolder& operator=(const TemporaryFolder&) = delete;

            /// @brief Removes the folder and what is in it.
            ~TemporaryFolder()
            {
                std::error_code ignored;
                std::filesystem::remove_all(path_, ignored);
            }

            /// @brief Writes a file of this folder.
            /// @param name The file's name.
            /// @param text The file's text.
            /// @return The file's path.
            std::string write(const std::string& name, const std::string& text) const
            {
                const std::filesystem::path file = path_ / name;
                std::ofstream out(file, std::ios::binary);
                out << text;
                return file.string();
            }

            /// @brief The path of a file this folder does not hold.
            std::string missing(const std::string& name) const
            {
                return (path_ / name).string();
            }

        private:
            static inline std::atomic<unsigned> next_index_{0};
            std::filesystem::path path_;
    };
} // namespace

TEST(Small_StartupRig, AnUnsetVariableMeansNoRig)
{
    const InMemoryRigFileReader reader;

    EXPECT_FALSE(read_startup_rig(nullptr, reader).has_value());
}

TEST(Small_StartupRig, AnEmptyVariableMeansNoRig)
{
    const InMemoryRigFileReader reader;

    EXPECT_FALSE(read_startup_rig("", reader).has_value());
}

TEST(Small_StartupRig, NoVariableWritesNoRecord)
{
    const InMemoryRigFileReader reader;
    SushiDSP::LogCapture capture;

    static_cast<void>(read_startup_rig(nullptr, reader));

    EXPECT_TRUE(capture.lines().empty());
}

TEST(Small_StartupRig, AFileTheReaderHoldsIsReadWhole)
{
    InMemoryRigFileReader reader;
    reader.add_file(K_PATH, K_TEXT);

    const std::optional<std::string> text = read_startup_rig(K_PATH.c_str(), reader);

    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, K_TEXT);
}

TEST(Small_StartupRig, AFileThatWillNotOpenGivesNoRig)
{
    const InMemoryRigFileReader reader;

    EXPECT_FALSE(read_startup_rig(K_PATH.c_str(), reader).has_value());
}

TEST(Small_StartupRig, AFileThatWillNotOpenWritesOneWarnNamingThePath)
{
    const InMemoryRigFileReader reader;
    SushiDSP::LogCapture capture;

    static_cast<void>(read_startup_rig(K_PATH.c_str(), reader));

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "plugin");
    EXPECT_NE(lines[0].message.find(K_PATH), std::string::npos) << lines[0].message;
}

TEST(Medium_StartupRig, AFileThatExistsIsReadWhole)
{
    const TemporaryFolder folder;
    const std::string path = folder.write("startup_rig.json", K_TEXT);

    const std::optional<std::string> text = read_startup_rig(path.c_str(), FileRigFileReader{});

    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, K_TEXT);
}

TEST(Medium_StartupRig, AFileThatDoesNotExistGivesNoRig)
{
    const TemporaryFolder folder;
    const std::string path = folder.missing("no_such_rig.json");
    SushiDSP::LogCapture capture;

    EXPECT_FALSE(read_startup_rig(path.c_str(), FileRigFileReader{}).has_value());
}

TEST(Medium_StartupRig, TheDefaultReaderTakesThePathFromTheEnvironment)
{
    const TemporaryFolder folder;
    const EnvironmentValue environment(folder.write("startup_rig.json", K_TEXT));

    const std::optional<std::string> text = read_startup_rig();

    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, K_TEXT);
}
