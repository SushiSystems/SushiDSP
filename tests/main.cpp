/**************************************************************************/
/* main.cpp                                                               */
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
 * @file main.cpp
 * @brief Runs the SushiDSP test suite under GoogleTest.
 * @author Mustafa Garip
 */

#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include <SushiLog/log/log_calls.hpp>

namespace
{
    /// @brief Reports whether a part of a suite name starts with a size: Small_, Medium_ or Large_.
    bool starts_with_size(const char* part)
    {
        for (const char* prefix : {"Small_", "Medium_", "Large_"})
        {
            if (std::strncmp(part, prefix, std::strlen(prefix)) == 0)
                return true;
        }
        return false;
    }

    /// @brief Reports whether a suite name carries a size, as `Small_X`, `Rows/Small_X` or `Small_X/0`.
    bool has_size_prefix(const char* suite)
    {
        if (starts_with_size(suite))
            return true;
        const char* slash = std::strchr(suite, '/');
        return slash != nullptr && starts_with_size(slash + 1);
    }

    /// @brief Prints every suite whose name carries no size.
    /// @return The number of such suites.
    int report_unsized_suites()
    {
        const ::testing::UnitTest& unit_test = *::testing::UnitTest::GetInstance();
        int unsized = 0;
        for (int index = 0; index < unit_test.total_test_suite_count(); ++index)
        {
            const char* suite = unit_test.GetTestSuite(index)->name();
            if (has_size_prefix(suite))
                continue;
            std::fprintf(stderr, "test suite '%s' has no size prefix: Small_, Medium_ or Large_\n",
                         suite);
            ++unsized;
        }
        return unsized;
    }
} // namespace

/**
 * @brief Runs every registered GoogleTest case, after refusing a suite with no size.
 * Records reach a test's sink before the call that logs them returns.
 * @return 0 on success, non-zero if any test fails or a suite carries no size.
 */
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    SushiLog::set_log_synchronous(true);
    if (report_unsized_suites() > 0)
        return 2;
    return RUN_ALL_TESTS();
}
