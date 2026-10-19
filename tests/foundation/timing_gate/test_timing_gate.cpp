/**************************************************************************/
/* test_timing_gate.cpp                                                   */
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
 * @file test_timing_gate.cpp
 * @brief Tests that the timing gate reports an optimised build exactly when NDEBUG is defined and a sanitiser build exactly when one is on.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/timing_gate/timing_gate.hpp>

TEST(Small_TimingGate, ReportsOptimisationFromNdebug)
{
#ifdef NDEBUG
    EXPECT_TRUE(SushiDSP::test::is_optimised_build());
#else
    EXPECT_FALSE(SushiDSP::test::is_optimised_build());
#endif
}

TEST(Small_TimingGate, ReportsSanitisationFromTheCompilerMacros)
{
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    EXPECT_TRUE(SushiDSP::test::is_sanitised_build());
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    EXPECT_TRUE(SushiDSP::test::is_sanitised_build());
#else
    EXPECT_FALSE(SushiDSP::test::is_sanitised_build());
#endif
#else
    EXPECT_FALSE(SushiDSP::test::is_sanitised_build());
#endif
}
