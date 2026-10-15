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
 * @file test_regression_test.cpp
 * @brief Tests that the regression macro declares a GoogleTest case.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/regression_test/regression_test.hpp>

SUSHIDSP_REGRESSION_TEST(RegressionTest, DeclaresAGoogleTestCase)
{
    EXPECT_TRUE(true);
}
