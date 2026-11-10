/**************************************************************************/
/* test_process_environment.cpp                                           */
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
 * @file test_process_environment.cpp
 * @brief Tests the EnvironmentReader over the environment of the process.
 * @author Mustafa Garip
 */

#include <cstdlib>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/process_environment.hpp>

namespace
{
    /// @brief Names a variable only this file sets.
    constexpr const char* K_SET_VARIABLE = "SUSHIDSP_PROCESS_ENVIRONMENT_TEST_SET";

    /// @brief Names a variable no test sets.
    constexpr const char* K_UNSET_VARIABLE = "SUSHIDSP_PROCESS_ENVIRONMENT_TEST_UNSET";

    /// @brief Sets the process variable @p name to @p value.
    void set_process_variable(const char* name, const char* value)
    {
#if defined(_WIN32)
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }
} // namespace

TEST(Medium_ProcessEnvironment, ReadsTheValueOfAVariableTheProcessHolds)
{
    set_process_variable(K_SET_VARIABLE, "warn,room=debug");
    const SushiDSP::ProcessEnvironment environment;

    EXPECT_EQ(environment.read(K_SET_VARIABLE), std::optional<std::string>("warn,room=debug"));
}

TEST(Medium_ProcessEnvironment, ReadsNothingForAVariableTheProcessDoesNotHold)
{
    const SushiDSP::ProcessEnvironment environment;

    EXPECT_FALSE(environment.read(K_UNSET_VARIABLE).has_value());
}
