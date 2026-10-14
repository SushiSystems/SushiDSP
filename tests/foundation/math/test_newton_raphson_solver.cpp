/**************************************************************************/
/* test_newton_raphson_solver.cpp                                         */
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
 * @file test_newton_raphson_solver.cpp
 * @brief Tests the convergence of NewtonRaphsonSolver.
 * @author Mustafa Garip
 */

#include <cmath>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/newton_raphson_solver.hpp>

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverFindsSquareRootOfTwo)
{
    auto function = [](float x)
    {
        return x * x - 2.0f;
    };
    auto derivative = [](float x)
    {
        return 2.0f * x;
    };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.0f, 6);

    EXPECT_NEAR(root, std::sqrt(2.0f), 1e-4f);
}

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverConvergesFromNegativeGuess)
{
    auto function = [](float x)
    {
        return x * x - 2.0f;
    };
    auto derivative = [](float x)
    {
        return 2.0f * x;
    };

    const float root = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, -1.0f, 6);

    EXPECT_NEAR(root, -std::sqrt(2.0f), 1e-4f);
}

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverHandlesNearZeroDerivativeWithoutDivergingToNanOrInf)
{
    // f(x) = x^3, root at 0, derivative 3x^2 vanishes exactly at the root.
    auto function = [](float x)
    {
        return x * x * x;
    };
    auto derivative = [](float x)
    {
        return 3.0f * x * x;
    };

    const float result = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.5f, 8);

    EXPECT_TRUE(std::isfinite(result));
}

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverWarmStartConvergesInFewerIterationsThanColdStart)
{
    auto function = [](float x)
    {
        return x * x - 2.0f;
    };
    auto derivative = [](float x)
    {
        return 2.0f * x;
    };

    const float warm = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 1.4f, 2);
    const float cold = SushiDSP::NewtonRaphsonSolver::solve(function, derivative, 0.01f, 2);

    const float target = std::sqrt(2.0f);
    EXPECT_LT(std::abs(warm - target), std::abs(cold - target));
}

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverJointOverloadMatchesSplitOverloadBitForBit)
{
    // Both overloads must walk identical iterates. See tests/foundation/math/README.md.
    auto function = [](float x)
    {
        return std::exp(x) - 3.0f;
    };
    auto derivative = [](float x)
    {
        return std::exp(x);
    };
    auto joint = [](float x)
    {
        const float e = std::exp(x);
        return SushiDSP::NewtonRaphsonSolver::Evaluation{e - 3.0f, e};
    };
    for (float guess = -2.0f; guess <= 4.0f; guess += 0.37f)
    {
        const float split =
            SushiDSP::NewtonRaphsonSolver::solve(function, derivative, guess, 4, -5.0f, 5.0f);
        const float shared =
            SushiDSP::NewtonRaphsonSolver::solve_joint(joint, guess, 4, -5.0f, 5.0f);
        EXPECT_EQ(split, shared);
    }
}

TEST(NewtonRaphsonSolver, NewtonRaphsonSolverJointOverloadEvaluatesOncePerIteration)
{
    int evaluations = 0;
    auto joint = [&evaluations](float x)
    {
        ++evaluations;
        return SushiDSP::NewtonRaphsonSolver::Evaluation{x * x - 2.0f, 2.0f * x};
    };
    SushiDSP::NewtonRaphsonSolver::solve_joint(joint, 1.0f, 4);
    EXPECT_EQ(evaluations, 4);
}
