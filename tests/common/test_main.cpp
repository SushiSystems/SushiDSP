/****************************************************************/
/* test_main.cpp                                                */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include <gtest/gtest.h>

/**
 * @brief Entry point for the SushiDSP test suite.
 *
 * Initializes GoogleTest and runs every registered case. SushiDSP is a
 * portable, dependency-free DSP library, so the tests exercise the real
 * kernels and graph directly on the host — there are no mocks and no device
 * setup to perform here.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 on success, non-zero if any test fails.
 */
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
