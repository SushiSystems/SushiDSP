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
#include <cstring>
#include <limits>

#include <gtest/gtest.h>

#include <SushiDSP/foundation/math/newton_raphson_solver.hpp>

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverFindsSquareRootOfTwo)
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

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverConvergesFromNegativeGuess)
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

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverHandlesNearZeroDerivativeWithoutDivergingToNanOrInf)
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

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverWarmStartConvergesInFewerIterationsThanColdStart)
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

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverJointOverloadMatchesSplitOverloadBitForBit)
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

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverJointOverloadEvaluatesOncePerIteration)
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

TEST(Small_NewtonRaphsonSolverReport, AFixedPointEndsTheSolveAndIsReported)
{
    auto function = [](float x)
    {
        return SushiDSP::NewtonRaphsonSolver::Evaluation{x * x - 4.0f, 2.0f * x};
    };
    SushiDSP::NewtonRaphsonSolver::Report report;

    const float root = SushiDSP::NewtonRaphsonSolver::solve_joint(
        function, 3.0f, 50, -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(), &report);

    EXPECT_NEAR(root, 2.0f, 1e-5f);
    EXPECT_EQ(report.ending, SushiDSP::NewtonRaphsonSolver::Ending::FixedPoint);
    EXPECT_GE(report.iterations, 2u);
    EXPECT_LT(report.iterations, 50u);
}

TEST(Small_NewtonRaphsonSolverReport, TheIterationCapIsReportedWithItsCount)
{
    auto function = [](float x)
    {
        return SushiDSP::NewtonRaphsonSolver::Evaluation{x * x - 4.0f, 2.0f * x};
    };
    SushiDSP::NewtonRaphsonSolver::Report report;

    static_cast<void>(SushiDSP::NewtonRaphsonSolver::solve_joint(
        function, 3.0f, 1, -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(), &report));

    EXPECT_EQ(report.ending, SushiDSP::NewtonRaphsonSolver::Ending::IterationCap);
    EXPECT_EQ(report.iterations, 1u);
}

TEST(Small_NewtonRaphsonSolverReport, ANonFiniteStepIsReportedAndKeepsTheInitialGuess)
{
    auto function = [](float)
    {
        return SushiDSP::NewtonRaphsonSolver::Evaluation{std::numeric_limits<float>::quiet_NaN(),
                                                         1.0f};
    };
    SushiDSP::NewtonRaphsonSolver::Report report;

    const float root = SushiDSP::NewtonRaphsonSolver::solve_joint(
        function, 1.5f, 8, -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(), &report);

    EXPECT_FLOAT_EQ(root, 1.5f);
    EXPECT_EQ(report.ending, SushiDSP::NewtonRaphsonSolver::Ending::NonFiniteStep);
    EXPECT_EQ(report.iterations, 1u);
}

TEST(Small_NewtonRaphsonSolverReport, ZeroAllowedIterationsReportZero)
{
    auto function = [](float x)
    {
        return SushiDSP::NewtonRaphsonSolver::Evaluation{x, 1.0f};
    };
    SushiDSP::NewtonRaphsonSolver::Report report;

    static_cast<void>(SushiDSP::NewtonRaphsonSolver::solve_joint(
        function, 1.0f, 0, -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(), &report));

    EXPECT_EQ(report.iterations, 0u);
    EXPECT_EQ(report.ending, SushiDSP::NewtonRaphsonSolver::Ending::IterationCap);
}

TEST(Small_NewtonRaphsonSolverReport, AskingForAReportLeavesTheRootBitForBit)
{
    auto function = [](float x)
    {
        return SushiDSP::NewtonRaphsonSolver::Evaluation{std::exp(x) - 3.0f, std::exp(x)};
    };
    SushiDSP::NewtonRaphsonSolver::Report report;

    const float without = SushiDSP::NewtonRaphsonSolver::solve_joint(function, 0.2f, 4);
    const float with = SushiDSP::NewtonRaphsonSolver::solve_joint(
        function, 0.2f, 4, -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(), &report);

    EXPECT_EQ(std::memcmp(&without, &with, sizeof(float)), 0);
}
