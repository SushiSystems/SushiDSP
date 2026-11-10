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
 * @file test_newton_raphson_solver.cpp
 * @brief Tests the convergence of NewtonRaphsonSolver.
 * @author Mustafa Garip
 */

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "support/tolerance.hpp"

#include <SushiDSP/foundation/math/newton_raphson_solver.hpp>

namespace
{
    using SushiDSP::NewtonRaphsonSolver;

    constexpr float K_NO_MINIMUM = -std::numeric_limits<float>::infinity();
    constexpr float K_NO_MAXIMUM = std::numeric_limits<float>::infinity();

    /// The rounded operations on the path of the last Newton step of x^2 - c: the square, the
    /// difference, the quotient and the update.
    constexpr std::size_t K_STEP_OPERATIONS = 4;

    float square_minus_two(float x)
    {
        return x * x - 2.0f;
    }

    float twice(float x)
    {
        return 2.0f * x;
    }

    /// @brief The joint evaluation of x^2 - 4, whose roots are plus and minus two.
    NewtonRaphsonSolver::Evaluation square_minus_four(float x)
    {
        return NewtonRaphsonSolver::Evaluation{x * x - 4.0f, 2.0f * x};
    }

    /// @brief The joint evaluation of exp(x) - 3.
    NewtonRaphsonSolver::Evaluation exponential_minus_three(float x)
    {
        const float e = std::exp(x);
        return NewtonRaphsonSolver::Evaluation{e - 3.0f, e};
    }

    /// One start for a solve of x^2 = 2: the guess and the root it must reach.
    struct SquareRootRow
    {
            const char* name;
            float guess;
            double root;
    };

    const SquareRootRow kSquareRootRows[] = {
        {"PositiveGuess", 1.0f, 1.41421356237309504880},
        {"NegativeGuess", -1.0f, -1.41421356237309504880},
    };

    /// One start of the split-versus-joint comparison.
    struct GuessRow
    {
            const char* name;
            float guess;
    };

    const GuessRow kGuessRows[] = {
        {"MinusTwo", -2.0f}, {"MinusOne", -1.0f}, {"Zero", 0.0f},  {"Half", 0.5f}, {"One", 1.0f},
        {"Two", 2.0f},       {"Three", 3.0f},     {"Four", 4.0f},
    };

    std::string square_root_name(const ::testing::TestParamInfo<SquareRootRow>& info)
    {
        return info.param.name;
    }

    std::string guess_name(const ::testing::TestParamInfo<GuessRow>& info)
    {
        return info.param.name;
    }

    class Small_NewtonRaphsonSolverSquareRoot : public ::testing::TestWithParam<SquareRootRow>
    {
    };

    class Small_NewtonRaphsonSolverGuess : public ::testing::TestWithParam<GuessRow>
    {
    };

    /// @brief Solves x^2 - 4 from 3 with the joint form and an unbounded range, filling the report.
    float solve_square_minus_four(std::uint32_t max_iterations, NewtonRaphsonSolver::Report& report)
    {
        return NewtonRaphsonSolver::solve_joint(square_minus_four, 3.0f, max_iterations, K_NO_MINIMUM, K_NO_MAXIMUM,
                                                &report);
    }
} // namespace

// Four rounded operations decide the last step, and six steps from 1 leave a truncation error far below it.
TEST_P(Small_NewtonRaphsonSolverSquareRoot, ConvergesToTheSquareRootOfTwo)
{
    const float root = NewtonRaphsonSolver::solve(square_minus_two, twice, GetParam().guess, 6);

    EXPECT_TRUE(TestSupport::is_within(
        TestSupport::measure_relative_error<float>(root, GetParam().root, K_STEP_OPERATIONS)));
}

INSTANTIATE_TEST_SUITE_P(Guesses, Small_NewtonRaphsonSolverSquareRoot, ::testing::ValuesIn(kSquareRootRows),
                         square_root_name);

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

    const float result = NewtonRaphsonSolver::solve(function, derivative, 0.5f, 8);

    EXPECT_TRUE(std::isfinite(result));
}

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverWarmStartConvergesInFewerIterationsThanColdStart)
{
    const float warm = NewtonRaphsonSolver::solve(square_minus_two, twice, 1.4f, 2);
    const float cold = NewtonRaphsonSolver::solve(square_minus_two, twice, 0.01f, 2);

    const float target = std::sqrt(2.0f);
    EXPECT_LT(std::abs(warm - target), std::abs(cold - target));
}

// Both overloads must walk identical iterates. See tests/foundation/math/README.md.
TEST_P(Small_NewtonRaphsonSolverGuess, JointOverloadMatchesSplitOverloadBitForBit)
{
    auto function = [](float x)
    {
        return std::exp(x) - 3.0f;
    };
    auto derivative = [](float x)
    {
        return std::exp(x);
    };

    const float split = NewtonRaphsonSolver::solve(function, derivative, GetParam().guess, 4, -5.0f, 5.0f);
    const float shared = NewtonRaphsonSolver::solve_joint(exponential_minus_three, GetParam().guess, 4, -5.0f, 5.0f);

    EXPECT_EQ(split, shared);
}

INSTANTIATE_TEST_SUITE_P(Guesses, Small_NewtonRaphsonSolverGuess, ::testing::ValuesIn(kGuessRows), guess_name);

TEST(Small_NewtonRaphsonSolver, NewtonRaphsonSolverJointOverloadEvaluatesOncePerIteration)
{
    int evaluations = 0;
    auto joint = [&evaluations](float x)
    {
        ++evaluations;
        return NewtonRaphsonSolver::Evaluation{x * x - 2.0f, 2.0f * x};
    };
    NewtonRaphsonSolver::solve_joint(joint, 1.0f, 4);
    EXPECT_EQ(evaluations, 4);
}

// From 3 the iterates 3, 2.1667, 2.0064, 2.00001 reach 2 within the four rounded operations of a step.
TEST(Small_NewtonRaphsonSolverReport, AFixedPointEndsTheSolveAtTheRoot)
{
    NewtonRaphsonSolver::Report report;

    const float root = solve_square_minus_four(50, report);

    EXPECT_TRUE(TestSupport::is_within(TestSupport::measure_relative_error<float>(root, 2.0, K_STEP_OPERATIONS)));
}

TEST(Small_NewtonRaphsonSolverReport, AFixedPointIsReportedAsFixedPoint)
{
    NewtonRaphsonSolver::Report report;

    static_cast<void>(solve_square_minus_four(50, report));

    EXPECT_EQ(report.ending, NewtonRaphsonSolver::Ending::FixedPoint);
}

TEST(Small_NewtonRaphsonSolverReport, AFixedPointEndsBeforeTheIterationBudget)
{
    NewtonRaphsonSolver::Report report;

    static_cast<void>(solve_square_minus_four(50, report));

    EXPECT_GE(report.iterations, 2u);
    EXPECT_LT(report.iterations, 50u);
}

TEST(Small_NewtonRaphsonSolverReport, TheIterationCapIsReportedWithItsCount)
{
    NewtonRaphsonSolver::Report report;

    static_cast<void>(solve_square_minus_four(1, report));

    EXPECT_EQ(report.ending, NewtonRaphsonSolver::Ending::IterationCap);
    EXPECT_EQ(report.iterations, 1u);
}

TEST(Small_NewtonRaphsonSolverReport, ANonFiniteStepIsReportedAndKeepsTheInitialGuess)
{
    auto function = [](float)
    {
        return NewtonRaphsonSolver::Evaluation{std::numeric_limits<float>::quiet_NaN(), 1.0f};
    };
    NewtonRaphsonSolver::Report report;

    const float root = NewtonRaphsonSolver::solve_joint(function, 1.5f, 8, K_NO_MINIMUM, K_NO_MAXIMUM, &report);

    EXPECT_FLOAT_EQ(root, 1.5f);
    EXPECT_EQ(report.ending, NewtonRaphsonSolver::Ending::NonFiniteStep);
    EXPECT_EQ(report.iterations, 1u);
}

TEST(Small_NewtonRaphsonSolverReport, ZeroAllowedIterationsReportZero)
{
    auto function = [](float x)
    {
        return NewtonRaphsonSolver::Evaluation{x, 1.0f};
    };
    NewtonRaphsonSolver::Report report;

    static_cast<void>(NewtonRaphsonSolver::solve_joint(function, 1.0f, 0, K_NO_MINIMUM, K_NO_MAXIMUM, &report));

    EXPECT_EQ(report.iterations, 0u);
    EXPECT_EQ(report.ending, NewtonRaphsonSolver::Ending::IterationCap);
}

TEST(Small_NewtonRaphsonSolverReport, AskingForAReportLeavesTheRootBitForBit)
{
    NewtonRaphsonSolver::Report report;

    const float without = NewtonRaphsonSolver::solve_joint(exponential_minus_three, 0.2f, 4);
    const float with =
        NewtonRaphsonSolver::solve_joint(exponential_minus_three, 0.2f, 4, K_NO_MINIMUM, K_NO_MAXIMUM, &report);

    EXPECT_EQ(std::memcmp(&without, &with, sizeof(float)), 0);
}
