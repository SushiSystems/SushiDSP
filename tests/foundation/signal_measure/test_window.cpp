/**************************************************************************/
/* test_window.cpp                                                        */
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
 * @file test_window.cpp
 * @brief Tests that the Blackman-Harris window is symmetric, near zero at its ends and unity at its centre.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/signal_measure/window.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

TEST(Small_Window, HasTheRequestedSize)
{
    EXPECT_EQ(SushiDSP::measure::blackman_harris(1024).size(), 1024u);
}

TEST(Small_Window, IsNearZeroAtBothEnds)
{
    const std::vector<float> window = SushiDSP::measure::blackman_harris(1024);
    EXPECT_NEAR(window.front(), 0.0f, 1e-4f);
    EXPECT_NEAR(window.back(), 0.0f, 1e-4f);
}

TEST(Small_Window, IsSymmetricAboutItsCentre)
{
    const std::vector<float> window = SushiDSP::measure::blackman_harris(513);
    for (std::size_t i = 0; i < window.size() / 2; ++i)
    {
        EXPECT_NEAR(window[i], window[window.size() - 1 - i], 1e-6f);
    }
}

TEST(Small_Window, PeaksAtUnityInTheMiddle)
{
    const std::vector<float> window = SushiDSP::measure::blackman_harris(513);
    EXPECT_NEAR(window[256], 1.0f, 1e-4f);
}
