/**************************************************************************/
/* test_newton_raphson_solver.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/math/NewtonRaphsonSolver.hpp"

TEST(Unit_Dsp, NewtonRaphsonSolverFindsSquareRootOfTwo)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.0f, 6);

    EXPECT_NEAR(root, std::sqrt(2.0f), 1e-4f);
}

TEST(Unit_Dsp, NewtonRaphsonSolverConvergesFromNegativeGuess)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, -1.0f, 6);

    EXPECT_NEAR(root, -std::sqrt(2.0f), 1e-4f);
}

TEST(Unit_Dsp, NewtonRaphsonSolverHandlesNearZeroDerivativeWithoutDivergingToNanOrInf)
{
    // f(x) = x^3, root at 0, derivative 3x^2 vanishes exactly at the root.
    auto function = [](float x) { return x * x * x; };
    auto derivative = [](float x) { return 3.0f * x * x; };

    const float result = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.5f, 8);

    EXPECT_TRUE(std::isfinite(result));
}

TEST(Unit_Dsp, NewtonRaphsonSolverWarmStartConvergesInFewerIterationsThanColdStart)
{
    auto function = [](float x) { return x * x - 2.0f; };
    auto derivative = [](float x) { return 2.0f * x; };

    const float warm = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.4f, 2);
    const float cold = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.01f, 2);

    const float target = std::sqrt(2.0f);
    EXPECT_LT(std::abs(warm - target), std::abs(cold - target));
}

TEST(Unit_Dsp, NewtonRaphsonSolverJointOverloadMatchesSplitOverloadBitForBit)
{
    // The circuit stages moved to the joint form to share one exp() between residual and
    // derivative; that must be a pure cost change, so both forms must walk identical iterates.
    auto function = [](float x) { return std::exp(x) - 3.0f; };
    auto derivative = [](float x) { return std::exp(x); };
    auto joint = [](float x)
    {
        const float e = std::exp(x);
        return SushiDSP::NewtonRaphsonSolver::Evaluation{ e - 3.0f, e };
    };
    for (float guess = -2.0f; guess <= 4.0f; guess += 0.37f)
    {
        const float split = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, guess, 4, -5.0f, 5.0f);
        const float shared = SushiDSP::NewtonRaphsonSolver::solve_joint(joint, guess, 4, -5.0f, 5.0f);
        EXPECT_EQ(split, shared);
    }
}

TEST(Unit_Dsp, NewtonRaphsonSolverJointOverloadEvaluatesOncePerIteration)
{
    int evaluations = 0;
    auto joint = [&evaluations](float x)
    {
        ++evaluations;
        return SushiDSP::NewtonRaphsonSolver::Evaluation{ x * x - 2.0f, 2.0f * x };
    };
    SushiDSP::NewtonRaphsonSolver::solve_joint(joint, 1.0f, 4);
    EXPECT_EQ(evaluations, 4);
}
