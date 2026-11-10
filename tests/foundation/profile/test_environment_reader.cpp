/**************************************************************************/
/* test_environment_reader.cpp                                            */
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
 * @file test_environment_reader.cpp
 * @brief Tests the environment reader port through its in-memory adapter and the process one.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/profile/environment_reader.hpp>

namespace
{
    /// @brief Holds a variable name that no environment sets.
    constexpr const char* K_UNSET_VARIABLE = "SUSHIDSP_PROFILE_TEST_VARIABLE_THAT_IS_NEVER_SET";

    /// @brief Expects @p reader to read a variable that nothing set as an empty string.
    void expect_an_unset_variable_reads_empty(const SushiDSP::profile::EnvironmentReader& reader)
    {
        EXPECT_TRUE(reader.value_of(K_UNSET_VARIABLE).empty());
    }
} // namespace

TEST(Small_InMemoryEnvironment, AVariableThatWasSetReadsBackItsValue)
{
    SushiDSP::profile::InMemoryEnvironment environment;
    environment.set("SOME_VARIABLE", "some value");

    EXPECT_EQ(environment.value_of("SOME_VARIABLE"), "some value");
}

TEST(Small_InMemoryEnvironment, AVariableThatWasNotSetReadsEmpty)
{
    const SushiDSP::profile::InMemoryEnvironment environment;

    expect_an_unset_variable_reads_empty(environment);
}

TEST(Medium_ProcessEnvironment, AVariableThatNothingSetReadsEmpty)
{
    expect_an_unset_variable_reads_empty(SushiDSP::profile::process_environment());
}
