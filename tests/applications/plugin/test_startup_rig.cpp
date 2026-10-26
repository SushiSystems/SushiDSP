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
 * @file test_startup_rig.cpp
 * @brief Tests the reader of the startup rig, and the record it writes for a file that will not open.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log_capture/log_capture.hpp>

#include "startup_rig.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
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
} // namespace

TEST(Medium_StartupRig, NoVariableMeansNoRigAndNoRecord)
{
    const EnvironmentValue environment("");
    SushiDSP::LogCapture capture;

    EXPECT_FALSE(SushiDSP::Plugin::read_startup_rig().has_value());
    EXPECT_TRUE(capture.lines().empty());
}

TEST(Medium_StartupRig, AFileThatExistsIsReadWhole)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "sushidsp_startup_rig.json";
    {
        std::ofstream file(path, std::ios::binary);
        file << "{\"rig\": 1}";
    }
    const EnvironmentValue environment(path.string());

    const std::optional<std::string> text = SushiDSP::Plugin::read_startup_rig();

    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "{\"rig\": 1}");
    std::filesystem::remove(path);
}

TEST(Medium_StartupRig, AFileThatWillNotOpenWritesOneWarnNamingThePath)
{
    const std::string path =
        (std::filesystem::temp_directory_path() / "sushidsp_no_such_rig.json").string();
    std::filesystem::remove(path);
    const EnvironmentValue environment(path);
    SushiDSP::LogCapture capture;

    EXPECT_FALSE(SushiDSP::Plugin::read_startup_rig().has_value());

    const std::vector<SushiDSP::CapturedLogLine> lines = capture.lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].level, SushiLog::LogLevel::Warn);
    EXPECT_EQ(lines[0].category, "plugin");
    EXPECT_NE(lines[0].message.find(path), std::string::npos) << lines[0].message;
}
