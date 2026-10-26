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
 * @file test_compiled_out.cpp
 * @brief Tests that the recording macros compile to nothing and pull in no SushiProfiler header.
 * @author Mustafa Garip
 */

// The generated config comes first, and this file then sees a build with the profiler off.
#include <SushiDSP/foundation/profile/profile_config.hpp>

#undef SD_ENABLE_PROFILER
#define SD_ENABLE_PROFILER 0

#include <SushiDSP/foundation/profile/profile.hpp>

#include <gtest/gtest.h>

#ifdef SP_ZONE
#error "profile.hpp included a SushiProfiler header with SD_ENABLE_PROFILER 0"
#endif

TEST(Small_ProfileCompiledOut, EveryMacroIsAStatementThatDoesNothing)
{
    SD_ZONE("test.zone");
    SD_ZONE_GROUP("test.zone", "Test");
    SD_ZONE_FINE("test.fine");
    SD_INSTANT("test.instant");
    SUCCEED();
}

TEST(Small_ProfileCompiledOut, ACounterDoesNotEvaluateItsValue)
{
    int evaluations = 0;
    SD_COUNTER("test.counter", "count", Add, static_cast<double>(++evaluations));
    EXPECT_EQ(evaluations, 0);
}

TEST(Small_ProfileCompiledOut, ALaneAndANamedZoneDoNotEvaluateTheirIds)
{
    int evaluations = 0;
    SD_LANE(++evaluations);
    SD_ZONE_NAMED("test.named", "Test", ++evaluations);
    EXPECT_EQ(evaluations, 0);
}
