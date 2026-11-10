/**************************************************************************/
/* test_in_memory_environment.cpp                                         */
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
 * @file test_in_memory_environment.cpp
 * @brief Tests the in-memory EnvironmentReader.
 * @author Mustafa Garip
 */

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/log/in_memory_environment.hpp>

TEST(Small_InMemoryEnvironment, ReadsTheValueOfAVariableItHolds)
{
    const SushiDSP::InMemoryEnvironment environment{{"FIRST", "one"}, {"SECOND", "two"}};

    EXPECT_EQ(environment.read("SECOND"), std::optional<std::string>("two"));
}

TEST(Small_InMemoryEnvironment, ReadsNothingForAVariableItDoesNotHold)
{
    const SushiDSP::InMemoryEnvironment environment{{"FIRST", "one"}};

    EXPECT_FALSE(environment.read("SECOND").has_value());
}
