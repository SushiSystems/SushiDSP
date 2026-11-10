/**************************************************************************/
/* test_compiled_out.cpp                                                  */
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
 * @file test_compiled_out.cpp
 * @brief Tests that the recording macros do not evaluate their arguments with the profiler off.
 * @author Mustafa Garip
 */

// The generated config comes first, and this file then sees a build with the profiler off.
#include <SushiDSP/foundation/profile/profile_config.hpp>

#undef SD_ENABLE_PROFILER
#define SD_ENABLE_PROFILER 0

#include <SushiDSP/foundation/profile/profile.hpp>

#include <gtest/gtest.h>

TEST(Small_ProfileCompiledOut, ACounterDoesNotEvaluateItsValue)
{
    int evaluations = 0;
    SD_COUNTER("test.counter", "count", Add, static_cast<double>(++evaluations));
    EXPECT_EQ(evaluations, 0);
}

TEST(Small_ProfileCompiledOut, ALaneDoesNotEvaluateItsId)
{
    int evaluations = 0;
    SD_LANE(++evaluations);
    EXPECT_EQ(evaluations, 0);
}

TEST(Small_ProfileCompiledOut, ANamedZoneDoesNotEvaluateItsId)
{
    int evaluations = 0;
    SD_ZONE_NAMED("test.named", "Test", ++evaluations);
    EXPECT_EQ(evaluations, 0);
}
