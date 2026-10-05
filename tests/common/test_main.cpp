/****************************************************************/
/* test_main.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_main.cpp
 * @brief Runs the SushiDSP test suite under GoogleTest.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

/**
 * @brief Runs every registered GoogleTest case.
 * @return 0 on success, non-zero if any test fails.
 */
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
