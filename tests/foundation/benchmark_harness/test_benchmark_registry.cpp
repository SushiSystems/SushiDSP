/**************************************************************************/
/* test_benchmark_registry.cpp                                            */
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
 * @file test_benchmark_registry.cpp
 * @brief Tests that the registry keeps every case and runs the ones the arguments select.
 * @author Mustafa Garip
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>

namespace
{
    std::vector<std::string> g_ran;

    const SushiDSP::bench::BenchmarkRegistration K_ZETA{"RegistryTestZeta",
                                                        [] { g_ran.push_back("Zeta"); }};
    const SushiDSP::bench::BenchmarkRegistration K_ALPHA{"RegistryTestAlpha",
                                                         [] { g_ran.push_back("Alpha"); }};
} // namespace

TEST(BenchmarkRegistry, BenchmarkRegistryKeepsEveryRegisteredCase)
{
    int found = 0;
    for (const SushiDSP::bench::BenchmarkCase& item : SushiDSP::bench::registered_cases())
    {
        if (std::string(item.group).rfind("RegistryTest", 0) == 0)
            ++found;
    }
    EXPECT_EQ(found, 2);
}

TEST(BenchmarkRegistry, BenchmarkRegistrySelectsEveryGroupWithoutArguments)
{
    const char* argv[] = {"bench"};
    EXPECT_TRUE(SushiDSP::bench::is_selected("Anything", 1, argv));
}

TEST(BenchmarkRegistry, BenchmarkRegistrySelectsAGroupThatHoldsAnArgument)
{
    const char* argv[] = {"bench", "Screamer"};
    EXPECT_TRUE(SushiDSP::bench::is_selected("TubeScreamerBlock", 2, argv));
    EXPECT_FALSE(SushiDSP::bench::is_selected("Jcm800", 2, argv));
}

TEST(BenchmarkRegistry, BenchmarkRegistryRunsTheSelectedGroupsInNameOrder)
{
    g_ran.clear();
    const char* argv[] = {"bench", "RegistryTest"};
    const int ran = SushiDSP::bench::run_selected(2, argv);

    EXPECT_EQ(ran, 2);
    EXPECT_EQ(g_ran, (std::vector<std::string>{"Alpha", "Zeta"}));
}
