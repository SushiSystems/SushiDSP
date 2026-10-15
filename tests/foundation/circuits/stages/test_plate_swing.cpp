/**************************************************************************/
/* test_plate_swing.cpp                                                   */
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
 * @file test_plate_swing.cpp
 * @brief Tests that the plate output stays inside its headroom and that its antiderivative matches it.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/stages/plate_swing.hpp>

#include <cmath>

namespace
{
    const SushiDSP::PlateSwing K_SWING{100.0f, 50.0f};
} // namespace

TEST(Small_PlateSwing, ZeroLinearVoltageGivesZeroPlate)
{
    EXPECT_NEAR(K_SWING.bound(0.0), 0.0, 1e-12);
    EXPECT_NEAR(K_SWING.bound(0.0f), 0.0f, 1e-6f);
}

TEST(Small_PlateSwing, LargeExcursionsSaturateAtTheirOwnHeadroom)
{
    EXPECT_NEAR(K_SWING.bound(1000.0), 100.0, 1e-6);
    EXPECT_NEAR(K_SWING.bound(-1000.0), -50.0, 1e-6);
}

TEST(Small_PlateSwing, OutputIsMonotonicAcrossZero)
{
    EXPECT_LT(K_SWING.bound(-2.0), K_SWING.bound(-1.0));
    EXPECT_LT(K_SWING.bound(-1.0), K_SWING.bound(0.5));
    EXPECT_LT(K_SWING.bound(0.5), K_SWING.bound(1.0));
}

TEST(Small_PlateSwing, AntiderivativeIsTheIntegralOfTheBound)
{
    const double h = 1e-4;
    for (double x : {-120.0, -20.0, 0.0, 20.0, 120.0})
    {
        const double slope = (K_SWING.bound_antiderivative(x + h) - K_SWING.bound_antiderivative(x - h)) / (2.0 * h);
        EXPECT_NEAR(slope, K_SWING.bound(x), 1e-3 * (1.0 + std::fabs(K_SWING.bound(x))));
    }
}
