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
    using SushiDSP::bench::BenchmarkCase;
    using SushiDSP::bench::BenchmarkRegistration;

    /// @brief Builds a case list whose bodies append their group's letter to a log, Zeta added first.
    class RegistryFixture
    {
        public:
            RegistryFixture()
                : zeta_(cases_, "Zeta", [this] { ran_.push_back("Zeta"); }),
                  alpha_(cases_, "Alpha", [this] { ran_.push_back("Alpha"); })
            {
            }

            const std::vector<BenchmarkCase>& cases() const
            {
                return cases_;
            }

            const std::vector<std::string>& ran() const
            {
                return ran_;
            }

        private:
            std::vector<BenchmarkCase> cases_;
            std::vector<std::string> ran_;
            BenchmarkRegistration zeta_;
            BenchmarkRegistration alpha_;
    };
} // namespace

TEST(Small_BenchmarkRegistry, BenchmarkRegistryKeepsEveryRegisteredCaseInRegistrationOrder)
{
    const RegistryFixture fixture;

    ASSERT_EQ(fixture.cases().size(), 2u);
    EXPECT_STREQ(fixture.cases()[0].group, "Zeta");
    EXPECT_STREQ(fixture.cases()[1].group, "Alpha");
}

TEST(Small_BenchmarkRegistry, BenchmarkRegistrySelectsEveryGroupWithoutArguments)
{
    const char* argv[] = {"bench"};
    EXPECT_TRUE(SushiDSP::bench::is_selected("Anything", 1, argv));
}

TEST(Small_BenchmarkRegistry, BenchmarkRegistrySelectsAGroupThatHoldsAnArgument)
{
    const char* argv[] = {"bench", "Screamer"};
    EXPECT_TRUE(SushiDSP::bench::is_selected("TubeScreamerBlock", 2, argv));
    EXPECT_FALSE(SushiDSP::bench::is_selected("Jcm800", 2, argv));
}

TEST(Small_BenchmarkRegistry, BenchmarkRegistryRunsTheSelectedGroupsInNameOrder)
{
    RegistryFixture fixture;
    const char* argv[] = {"bench"};

    const int ran = SushiDSP::bench::run_selected(fixture.cases(), 1, argv);

    EXPECT_EQ(ran, 2);
    EXPECT_EQ(fixture.ran(), (std::vector<std::string>{"Alpha", "Zeta"}));
}

TEST(Small_BenchmarkRegistry, BenchmarkRegistryRunsOnlyTheGroupsAnArgumentSelects)
{
    RegistryFixture fixture;
    const char* argv[] = {"bench", "Zet"};

    const int ran = SushiDSP::bench::run_selected(fixture.cases(), 2, argv);

    EXPECT_EQ(ran, 1);
    EXPECT_EQ(fixture.ran(), (std::vector<std::string>{"Zeta"}));
}
