/**************************************************************************/
/* test_soft_transition.cpp                                               */
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
 * @file test_soft_transition.cpp
 * @brief Tests the softplus and logistic functions the device models share.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/foundation/circuits/models/soft_transition.hpp>

#include <cmath>

TEST(Small_SoftTransition, SoftplusOfZeroIsLnTwo)
{
    EXPECT_NEAR(SushiDSP::models::softplus(0.0), std::log(2.0), 1e-12);
}

TEST(Small_SoftTransition, SoftplusIsTheIdentityAboveThirty)
{
    EXPECT_EQ(SushiDSP::models::softplus(40.0), 40.0);
}

TEST(Small_SoftTransition, LogisticOfZeroIsOneHalf)
{
    EXPECT_NEAR(SushiDSP::models::logistic(0.0), 0.5, 1e-12);
}

TEST(Small_SoftTransition, LogisticApproachesOneAndZeroAtTheExtremes)
{
    EXPECT_NEAR(SushiDSP::models::logistic(50.0), 1.0, 1e-12);
    EXPECT_NEAR(SushiDSP::models::logistic(-50.0), 0.0, 1e-12);
}

TEST(Small_SoftTransition, LogisticIsTheDerivativeOfSoftplus)
{
    const double t = 1.3;
    const double h = 1e-5;
    const double slope = (SushiDSP::models::softplus(t + h) - SushiDSP::models::softplus(t - h)) / (2.0 * h);
    EXPECT_NEAR(slope, SushiDSP::models::logistic(t), 1e-7);
}

TEST(Small_SoftTransition, JointFormAgreesWithTheSeparateFunctions)
{
    for (double t : {-4.0, -0.5, 0.0, 2.5, 35.0})
    {
        double softplus_value = 0.0;
        double logistic_value = 0.0;
        SushiDSP::models::softplus_and_logistic(t, softplus_value, logistic_value);
        EXPECT_NEAR(softplus_value, SushiDSP::models::softplus(t), 1e-12);
        EXPECT_NEAR(logistic_value, SushiDSP::models::logistic(t), 1e-12);
    }
}
