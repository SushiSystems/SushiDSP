/**************************************************************************/
/* test_regression_test.cpp                                               */
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
 * @file test_regression_test.cpp
 * @brief Tests that the regression macro declares a GoogleTest case.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/regression_test/regression_test.hpp>

SUSHIDSP_REGRESSION_TEST(Small_RegressionTest, DeclaresAGoogleTestCase)
{
    const ::testing::TestInfo* const info = ::testing::UnitTest::GetInstance()->current_test_info();

    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->test_suite_name(), "Small_RegressionTest");
    EXPECT_STREQ(info->name(), "DeclaresAGoogleTestCase");
}
